"""
Embody Mannequin Studio — Animation Diffusion Server
=====================================================
Text-conditioned DDPM trained on YOUR UE Mannequin data using your GPU.
Architecture: Motion Diffusion Transformer (MDM-style), DDIM inference.

  pip install -r requirements_server.txt
  python server.py
"""

import json, math, os, threading, time, traceback
from copy import deepcopy
from pathlib import Path
from typing import Dict, List, Optional, Any

import numpy as np
import torch
import torch.nn as nn
import torch.nn.functional as F
from torch.amp import GradScaler, autocast
import uvicorn
from fastapi import FastAPI, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel

# ── Config ────────────────────────────────────────────────────────────────────

PORT        = 8420
MODEL_DIR   = Path("anim_models");  MODEL_DIR.mkdir(exist_ok=True)
MAX_FRAMES  = 128          # sequence length the model operates on
D_MODEL     = 512
N_HEADS     = 8
N_LAYERS    = 8
DIFF_STEPS  = 1000         # training diffusion steps
DDIM_STEPS  = 50           # inference steps
CFG_SCALE   = 7.5          # classifier-free guidance strength
TEXT_DROP   = 0.10         # probability of dropping text during training (CFG)
EMA_DECAY   = 0.9999

# ── App ───────────────────────────────────────────────────────────────────────

app = FastAPI(title="Embody Animation Server")
app.add_middleware(CORSMiddleware, allow_origins=["*"], allow_methods=["*"], allow_headers=["*"])

# ── Global state ──────────────────────────────────────────────────────────────

_lock = threading.Lock()
_train_st: Dict[str, Any] = dict(
    status="idle", step=0, total=0, log="", error="", should_stop=False
)
_gen_st: Dict[str, Any] = dict(stage="idle", step=0, total=100)
_db: Dict[str, Any] = dict(
    bones=[], n_bones=0,
    records=[],           # list[AnimRecord]
    text_embs=None,       # (N, D)
    anim_names=[],
    norm_mean=None, norm_std=None,
    ema_model=None,       # EMA weights for inference
    model_cfg=None,
    model_name=None,
)
_text_enc = None
_enc_lock  = threading.Lock()


# ─────────────────────────────────────────────────────────────────────────────
# 1.  AnimRecord
# ─────────────────────────────────────────────────────────────────────────────

class AnimRecord:
    def __init__(self, name: str, fps: float, loop: bool, frames: np.ndarray):
        self.name   = name
        self.fps    = fps
        self.loop   = loop
        self.frames = frames.astype(np.float32)   # (T, B*3)


# ─────────────────────────────────────────────────────────────────────────────
# 2.  Text encoder  (sentence-transformers, frozen)
# ─────────────────────────────────────────────────────────────────────────────

class _FallbackEnc:
    D = 128
    def encode(self, texts, normalize_embeddings=True):
        out = np.zeros((len(texts), self.D), np.float32)
        for i, t in enumerate(texts):
            for w in t.lower().replace("-"," ").replace("_"," ").split():
                out[i, abs(hash(w)) % self.D] += 1.0
            n = np.linalg.norm(out[i])
            if n > 0: out[i] /= n
        return out

def _get_enc():
    global _text_enc
    with _enc_lock:
        if _text_enc is None:
            try:
                from sentence_transformers import SentenceTransformer
                _text_enc = SentenceTransformer("all-mpnet-base-v2")
                print("[Server] Text encoder: all-mpnet-base-v2 (768-dim)")
            except ImportError:
                print("[Server] sentence-transformers missing — using fallback encoder")
                _text_enc = _FallbackEnc()
    return _text_enc

def encode_text(texts: List[str]) -> np.ndarray:
    return _get_enc().encode(texts, normalize_embeddings=True)


# ─────────────────────────────────────────────────────────────────────────────
# 3.  Data augmentation  (essential for small datasets)
# ─────────────────────────────────────────────────────────────────────────────

# UE Mannequin left ↔ right bone pairs for mirror augmentation
_MIRROR_PAIRS = [
    ("clavicle_l","clavicle_r"),("upperarm_l","upperarm_r"),
    ("lowerarm_l","lowerarm_r"),("hand_l","hand_r"),
    ("thigh_l","thigh_r"),("calf_l","calf_r"),
    ("foot_l","foot_r"),("ball_l","ball_r"),
    ("thumb_01_l","thumb_01_r"),("thumb_02_l","thumb_02_r"),("thumb_03_l","thumb_03_r"),
    ("index_01_l","index_01_r"),("index_02_l","index_02_r"),("index_03_l","index_03_r"),
    ("middle_01_l","middle_01_r"),("middle_02_l","middle_02_r"),("middle_03_l","middle_03_r"),
    ("ring_01_l","ring_01_r"),("ring_02_l","ring_02_r"),("ring_03_l","ring_03_r"),
    ("pinky_01_l","pinky_01_r"),("pinky_02_l","pinky_02_r"),("pinky_03_l","pinky_03_r"),
]

def _build_mirror_idx(bones: List[str]) -> Optional[np.ndarray]:
    """Returns index array for swapping left/right columns, or None if no pairs found."""
    idx = list(range(len(bones)))
    found = False
    bone_map = {b: i for i, b in enumerate(bones)}
    for l, r in _MIRROR_PAIRS:
        if l in bone_map and r in bone_map:
            idx[bone_map[l]], idx[bone_map[r]] = bone_map[r], bone_map[l]
            found = True
    return np.array(idx) if found else None


def augment(frames: np.ndarray, mirror_idx: Optional[np.ndarray], rng: np.random.Generator) -> np.ndarray:
    """
    frames: (T, B*3)  — normalised
    Returns augmented copy.
    """
    T, D = frames.shape
    f = frames.copy()

    # 1. Time warp  (resample to different speed, then resize back to T)
    speed = rng.uniform(0.6, 1.4)
    new_T = max(2, int(round(T / speed)))
    if new_T != T:
        t_src = np.linspace(0, 1, T)
        t_dst = np.linspace(0, 1, new_T)
        warped = np.stack([np.interp(t_dst, t_src, f[:, c]) for c in range(D)], axis=1)
        # resize back to T
        t2 = np.linspace(0, 1, new_T)
        t3 = np.linspace(0, 1, T)
        f = np.stack([np.interp(t3, t2, warped[:, c]) for c in range(D)], axis=1)

    # 2. Additive rotation noise
    f += rng.normal(0, 0.05, f.shape).astype(np.float32)

    # 3. Global rotation scale
    scale = rng.uniform(0.85, 1.15)
    f *= scale

    # 4. Mirror  (swap left/right bones)
    if mirror_idx is not None and rng.random() < 0.5:
        # Expand bone index to column index (3 cols per bone)
        col_idx = np.concatenate([mirror_idx * 3, mirror_idx * 3 + 1, mirror_idx * 3 + 2])
        col_order = np.argsort(np.concatenate([np.arange(len(mirror_idx)) * 3,
                                               np.arange(len(mirror_idx)) * 3 + 1,
                                               np.arange(len(mirror_idx)) * 3 + 2]))
        # simpler: just reorder bone columns
        n_bones = D // 3
        f_bones = f.reshape(T, n_bones, 3)[:, mirror_idx, :]
        f = f_bones.reshape(T, D)

    # 5. Temporal reversal
    if rng.random() < 0.3:
        f = f[::-1].copy()

    return f.astype(np.float32)


# ─────────────────────────────────────────────────────────────────────────────
# 4.  Diffusion schedule
# ─────────────────────────────────────────────────────────────────────────────

def _cosine_betas(T: int, s: float = 0.008) -> torch.Tensor:
    t = torch.linspace(0, T, T + 1)
    alphas_bar = torch.cos(((t / T) + s) / (1 + s) * math.pi / 2) ** 2
    alphas_bar = alphas_bar / alphas_bar[0]
    betas = 1 - (alphas_bar[1:] / alphas_bar[:-1])
    return betas.clamp(0, 0.999)


class GaussianDiffusion:
    def __init__(self, T: int = DIFF_STEPS, device: torch.device = None):
        self.T      = T
        self.device = device or torch.device("cpu")
        betas = _cosine_betas(T).to(self.device)
        alphas = 1 - betas
        self.alphas_bar = torch.cumprod(alphas, dim=0)           # (T,)
        self.sqrt_ab    = self.alphas_bar.sqrt()
        self.sqrt_1mab  = (1 - self.alphas_bar).sqrt()

    def q_sample(self, x0: torch.Tensor, t: torch.Tensor, noise: torch.Tensor) -> torch.Tensor:
        """Add noise: x_t = sqrt(ā_t)*x0 + sqrt(1-ā_t)*noise"""
        ab  = self.sqrt_ab[t].view(-1, 1, 1)
        mab = self.sqrt_1mab[t].view(-1, 1, 1)
        return ab * x0 + mab * noise

    def loss(self, model: nn.Module, x0: torch.Tensor, text_emb: torch.Tensor,
             mask: torch.Tensor, use_amp: bool = False) -> torch.Tensor:
        """
        x0       : (B, T, D)   — clean, normalised motion
        text_emb : (B, text_D) — text embedding (zeroed out for uncond)
        mask     : (B, T)      — 1 = valid frame, 0 = padding
        """
        B = x0.shape[0]
        t = torch.randint(0, self.T, (B,), device=self.device)
        noise = torch.randn_like(x0)
        xt = self.q_sample(x0, t, noise)

        with autocast('cuda', enabled=use_amp):
            pred_x0 = model(xt, t, text_emb)

        # predict x0 directly (MDM-style)
        loss = F.mse_loss(pred_x0 * mask.unsqueeze(-1), x0 * mask.unsqueeze(-1))
        # auxiliary velocity loss
        if x0.shape[1] > 1:
            vel_pred = pred_x0[:, 1:] - pred_x0[:, :-1]
            vel_true = x0[:, 1:] - x0[:, :-1]
            m = mask[:, 1:].unsqueeze(-1)
            loss = loss + 0.1 * F.mse_loss(vel_pred * m, vel_true * m)
        return loss

    @torch.no_grad()
    def ddim_sample(self, model: nn.Module, text_emb: torch.Tensor,
                    T_out: int, guidance_scale: float = CFG_SCALE,
                    eta: float = 0.0) -> torch.Tensor:
        """
        DDIM sampling with classifier-free guidance.
        Returns (1, T_out, D) clean motion.
        """
        B, D_out = 1, model.out_dim
        device   = self.device

        # Zero embedding for unconditional branch
        uncond = torch.zeros_like(text_emb)

        # Concat cond + uncond for batched CFG
        text_both = torch.cat([text_emb, uncond], dim=0)   # (2, text_D)

        # DDIM timestep subsequence
        step_ratio = self.T // DDIM_STEPS
        timesteps  = list(reversed(range(0, self.T, step_ratio)))[:DDIM_STEPS]

        xt = torch.randn(B, T_out, D_out, device=device)

        for i, t_cur in enumerate(timesteps):
            t_prev = timesteps[i + 1] if i + 1 < len(timesteps) else 0
            t_tensor = torch.tensor([t_cur], device=device).expand(2)

            xt_in   = xt.expand(2, -1, -1)
            pred_x0 = model(xt_in, t_tensor, text_both)   # (2, T_out, D)

            # CFG
            pred_cond, pred_uncond = pred_x0[0], pred_x0[1]
            pred = pred_uncond + guidance_scale * (pred_cond - pred_uncond)

            # DDIM step
            ab_cur  = self.alphas_bar[t_cur]
            ab_prev = self.alphas_bar[t_prev] if t_prev > 0 else torch.tensor(1.0, device=device)
            sigma   = eta * ((1 - ab_prev) / (1 - ab_cur) * (1 - ab_cur / ab_prev)).sqrt()

            dir_xt  = (1 - ab_prev - sigma**2).sqrt() * (xt - ab_cur.sqrt() * pred) / (1 - ab_cur).sqrt()
            noise   = sigma * torch.randn_like(xt) if eta > 0 else 0
            xt      = ab_prev.sqrt() * pred + dir_xt + noise

        return xt  # (1, T_out, D)


# ─────────────────────────────────────────────────────────────────────────────
# 5.  Motion Diffusion Transformer  (MDT)
# ─────────────────────────────────────────────────────────────────────────────

class SinusoidalEmb(nn.Module):
    def __init__(self, dim: int):
        super().__init__()
        self.dim = dim

    def forward(self, t: torch.Tensor) -> torch.Tensor:
        """t: (B,) int → (B, dim)"""
        half = self.dim // 2
        freqs = torch.exp(-math.log(10000) * torch.arange(half, device=t.device) / (half - 1))
        args  = t.float().unsqueeze(1) * freqs.unsqueeze(0)
        return torch.cat([args.sin(), args.cos()], dim=-1)


class MDTLayer(nn.Module):
    def __init__(self, d: int, n_heads: int, dropout: float = 0.1):
        super().__init__()
        self.self_attn   = nn.MultiheadAttention(d, n_heads, dropout=dropout, batch_first=True)
        self.cross_attn  = nn.MultiheadAttention(d, n_heads, dropout=dropout, batch_first=True)
        self.ff          = nn.Sequential(
            nn.Linear(d, d * 4), nn.GELU(), nn.Dropout(dropout), nn.Linear(d * 4, d)
        )
        self.norm1 = nn.LayerNorm(d)
        self.norm2 = nn.LayerNorm(d)
        self.norm3 = nn.LayerNorm(d)
        self.drop  = nn.Dropout(dropout)

    def forward(self, x: torch.Tensor, ctx: torch.Tensor) -> torch.Tensor:
        # x: (B, T, d)  ctx: (B, ctx_len, d)
        x = self.norm1(x + self.drop(self.self_attn(x, x, x)[0]))
        x = self.norm2(x + self.drop(self.cross_attn(x, ctx, ctx)[0]))
        x = self.norm3(x + self.drop(self.ff(x)))
        return x


class MDT(nn.Module):
    def __init__(self, n_bones: int, text_dim: int,
                 d: int = D_MODEL, n_heads: int = N_HEADS, n_layers: int = N_LAYERS):
        super().__init__()
        in_dim   = n_bones * 3
        self.out_dim = in_dim

        # Motion input embedding
        self.motion_in  = nn.Linear(in_dim, d)
        self.pos_emb    = nn.Embedding(MAX_FRAMES + 8, d)

        # Timestep embedding
        self.time_emb   = nn.Sequential(
            SinusoidalEmb(d), nn.Linear(d, d * 2), nn.SiLU(), nn.Linear(d * 2, d)
        )

        # Text projection (text_dim → d, used as cross-attention context)
        self.text_proj  = nn.Sequential(
            nn.Linear(text_dim, d * 2), nn.SiLU(), nn.Linear(d * 2, d)
        )

        # Transformer layers
        self.layers     = nn.ModuleList([MDTLayer(d, n_heads) for _ in range(n_layers)])

        # Output
        self.out_norm   = nn.LayerNorm(d)
        self.out_head   = nn.Linear(d, in_dim)

        self._init_weights()

    def _init_weights(self):
        for p in self.parameters():
            if p.dim() > 1:
                nn.init.xavier_uniform_(p)
        nn.init.zeros_(self.out_head.weight)
        nn.init.zeros_(self.out_head.bias)

    def forward(self, xt: torch.Tensor, t: torch.Tensor, text_emb: torch.Tensor) -> torch.Tensor:
        """
        xt       : (B, T, in_dim)
        t        : (B,) int
        text_emb : (B, text_dim)
        → (B, T, in_dim)
        """
        B, T, _ = xt.shape
        device   = xt.device

        # Motion tokens
        x = self.motion_in(xt)                                       # (B, T, d)
        pos = torch.arange(T, device=device).unsqueeze(0)
        x = x + self.pos_emb(pos)                                    # (B, T, d)

        # Timestep: add to every token
        te = self.time_emb(t).unsqueeze(1)                           # (B, 1, d)
        x  = x + te

        # Text context: (B, 1, d) — single token used for cross-attention
        ctx = self.text_proj(text_emb).unsqueeze(1)                  # (B, 1, d)

        for layer in self.layers:
            x = layer(x, ctx)

        return self.out_head(self.out_norm(x))


# ─────────────────────────────────────────────────────────────────────────────
# 6.  EMA
# ─────────────────────────────────────────────────────────────────────────────

class EMA:
    def __init__(self, model: nn.Module, decay: float = EMA_DECAY):
        self.decay = decay
        self.shadow = deepcopy(model).eval()
        for p in self.shadow.parameters():
            p.requires_grad_(False)

    @torch.no_grad()
    def update(self, model: nn.Module):
        for s, m in zip(self.shadow.parameters(), model.parameters()):
            s.data.mul_(self.decay).add_(m.data, alpha=1 - self.decay)

    def state_dict(self):
        return self.shadow.state_dict()


# ─────────────────────────────────────────────────────────────────────────────
# 7.  Training loop
# ─────────────────────────────────────────────────────────────────────────────

def _resample(frames: np.ndarray, target_T: int) -> np.ndarray:
    T = frames.shape[0]
    if T == target_T:
        return frames
    t_src = np.linspace(0, 1, T)
    t_dst = np.linspace(0, 1, target_T)
    return np.stack([np.interp(t_dst, t_src, frames[:, c]) for c in range(frames.shape[1])], axis=1)


def _to_fixed_len(frames: np.ndarray) -> tuple:
    """Pad/crop to MAX_FRAMES, return (fixed_frames, mask)."""
    T = frames.shape[0]
    if T >= MAX_FRAMES:
        return frames[:MAX_FRAMES], np.ones(MAX_FRAMES, np.float32)
    pad_len = MAX_FRAMES - T
    padded  = np.concatenate([frames, np.zeros((pad_len, frames.shape[1]), np.float32)], axis=0)
    mask    = np.array([1.0] * T + [0.0] * pad_len, np.float32)
    return padded, mask


def _run_training(model_name: str, epochs: int, lr: float):
    def log(msg: str):
        with _lock: _train_st["log"] = msg
        print(f"[Train] {msg}")

    try:
        records  = _db["records"]
        bones    = _db["bones"]
        n_bones  = len(bones)
        if not records:
            raise ValueError("No training data.")

        log(f"Encoding text for {len(records)} sample(s)…")
        names      = [r.name for r in records]
        text_embs  = encode_text(names)                     # (N, D_text)
        text_dim   = text_embs.shape[1]
        _db["text_embs"]  = text_embs
        _db["anim_names"] = names

        # Normalise motion data
        all_f = np.concatenate([r.frames for r in records], axis=0)
        mean  = all_f.mean(0).astype(np.float32)
        std   = (all_f.std(0) + 1e-6).astype(np.float32)
        _db["norm_mean"] = mean
        _db["norm_std"]  = std

        # Model setup
        device    = torch.device("cuda" if torch.cuda.is_available() else "cpu")
        log(f"Device: {device}  |  n_bones: {n_bones}  |  text_dim: {text_dim}")
        model     = MDT(n_bones=n_bones, text_dim=text_dim).to(device)
        ema       = EMA(model)
        diffusion = GaussianDiffusion(T=DIFF_STEPS, device=device)
        opt       = torch.optim.AdamW(model.parameters(), lr=lr, weight_decay=1e-4)
        sched     = torch.optim.lr_scheduler.CosineAnnealingLR(opt, T_max=epochs, eta_min=lr * 0.01)
        use_amp   = device.type == "cuda"
        scaler    = GradScaler('cuda', enabled=use_amp)
        mirror_idx = _build_mirror_idx(bones)
        rng        = np.random.default_rng()

        # Pre-normalise records
        norm_records = []
        for r in records:
            nf = (r.frames - mean) / std
            norm_records.append((nf, r.fps, r.loop))

        AUGMENT_FACTOR = max(8, 200 // max(len(records), 1))
        log_every      = max(1, epochs // 20)
        _db["model_cfg"] = dict(n_bones=n_bones, text_dim=text_dim)

        log(f"Starting: {epochs} epochs  |  augment×{AUGMENT_FACTOR}  |  AMP={use_amp}")

        for epoch in range(epochs):
            with _lock:
                if _train_st["should_stop"]:
                    log("Stopped by user.")
                    _train_st["status"] = "idle"
                    return

            model.train()
            epoch_loss = 0.0
            n_steps    = 0

            # Each epoch: augment each record AUGMENT_FACTOR times
            for rec_idx, (nf, fps, loop) in enumerate(norm_records):
                txt_e = torch.tensor(text_embs[rec_idx], dtype=torch.float32, device=device).unsqueeze(0)

                for _ in range(AUGMENT_FACTOR):
                    aug   = augment(nf, mirror_idx, rng)
                    fixed, mask = _to_fixed_len(aug)
                    x0    = torch.tensor(fixed, dtype=torch.float32, device=device).unsqueeze(0)
                    m     = torch.tensor(mask,  dtype=torch.float32, device=device).unsqueeze(0)

                    # Classifier-free guidance: drop text with probability TEXT_DROP
                    if rng.random() < TEXT_DROP:
                        cond = torch.zeros_like(txt_e)
                    else:
                        cond = txt_e

                    opt.zero_grad()
                    loss = diffusion.loss(model, x0, cond, m, use_amp=use_amp)
                    scaler.scale(loss).backward()
                    scaler.unscale_(opt)
                    nn.utils.clip_grad_norm_(model.parameters(), 1.0)
                    scaler.step(opt)
                    scaler.update()
                    ema.update(model)
                    epoch_loss += loss.item()
                    n_steps    += 1

            sched.step()
            avg = epoch_loss / max(n_steps, 1)

            with _lock:
                _train_st["step"] = epoch + 1
                _train_st["log"]  = f"Epoch {epoch+1}/{epochs} — loss: {avg:.5f}"

            if (epoch + 1) % log_every == 0:
                log(f"Epoch {epoch+1}/{epochs} — loss: {avg:.5f}")

        # Save checkpoint
        ckpt_path = MODEL_DIR / f"{model_name}.pt"
        torch.save({
            "ema_state":  ema.state_dict(),
            "model_cfg":  dict(n_bones=n_bones, text_dim=text_dim,
                               d_model=D_MODEL, n_heads=N_HEADS, n_layers=N_LAYERS),
            "bones":      bones,
            "norm_mean":  mean,
            "norm_std":   std,
            "text_embs":  text_embs,
            "anim_names": names,
            "model_name": model_name,
        }, ckpt_path)

        _db["ema_model"]  = ema.shadow
        _db["model_name"] = model_name
        log(f"Done — saved {ckpt_path}")
        with _lock:
            _train_st.update(status="completed", step=epochs)

    except Exception as e:
        print(traceback.format_exc())
        with _lock:
            _train_st.update(status="error", error=str(e), log=f"Error: {e}")


# ─────────────────────────────────────────────────────────────────────────────
# 8.  Generation  (DDIM + CFG)
# ─────────────────────────────────────────────────────────────────────────────

def _generate(prompt: str, duration: float, fps: float, loop: bool) -> dict:
    bones   = _db["bones"]
    records = _db["records"]
    if not bones:
        raise HTTPException(400, "No training data. Train the model first.")

    n_bones = len(bones)
    T_out   = max(2, int(round(duration * fps)))

    _gen_st.update(stage="encoding", step=10)
    prompt_emb = encode_text([prompt])                          # (1, D_text)

    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")

    # ── Use EMA model if available ──
    ema_model: Optional[nn.Module] = _db.get("ema_model")
    if ema_model is not None:
        _gen_st.update(stage="diffusing", step=20)
        cfg   = _db["model_cfg"]
        model = ema_model.to(device).eval()

        text_emb_t = torch.tensor(prompt_emb, dtype=torch.float32, device=device)
        diffusion  = GaussianDiffusion(device=device)

        # Generate MAX_FRAMES, then resample to T_out
        with torch.no_grad():
            out = diffusion.ddim_sample(model, text_emb_t, MAX_FRAMES)   # (1, MAX_FRAMES, D)

        frames_norm = out.squeeze(0).cpu().numpy()                        # (MAX_FRAMES, D)
        _gen_st["step"] = 80

        # Denormalise
        mean = _db["norm_mean"]
        std  = _db["norm_std"]
        frames = frames_norm * std + mean                                  # (MAX_FRAMES, D)

        # Resample to requested length
        frames = _resample(frames, T_out)

    else:
        raise HTTPException(400, "No trained model found. Train a model first.")

    _gen_st.update(stage="extracting", step=90)

    # (T_out, B*3) → {bone: [{x,y,z}, ...]}
    result: Dict[str, List[Dict[str, float]]] = {}
    for b, bone in enumerate(bones):
        c = b * 3
        result[bone] = [
            {"x": float(frames[t, c]), "y": float(frames[t, c+1]), "z": float(frames[t, c+2])}
            for t in range(T_out)
        ]

    _gen_st.update(stage="done", step=100)
    return {"bones": result, "fps": float(fps), "frame_count": T_out, "loop": loop}


# ─────────────────────────────────────────────────────────────────────────────
# 9.  Auto-load most recent checkpoint on startup
# ─────────────────────────────────────────────────────────────────────────────

def _try_load_latest():
    ckpts = sorted(MODEL_DIR.glob("*.pt"), key=lambda p: p.stat().st_mtime, reverse=True)
    if not ckpts:
        return
    ckpt_path = ckpts[0]
    try:
        ck   = torch.load(ckpt_path, map_location="cpu")
        cfg  = ck["model_cfg"]
        model = MDT(n_bones=cfg["n_bones"], text_dim=cfg["text_dim"])
        model.load_state_dict(ck["ema_state"])
        model.eval()
        _db.update(
            bones      = ck["bones"],
            n_bones    = cfg["n_bones"],
            ema_model  = model,
            model_cfg  = cfg,
            norm_mean  = ck["norm_mean"],
            norm_std   = ck["norm_std"],
            text_embs  = ck["text_embs"],
            anim_names = ck["anim_names"],
            model_name = ck.get("model_name", ckpt_path.stem),
        )
        print(f"[Server] Loaded checkpoint: {ckpt_path.name}  "
              f"({cfg['n_bones']} bones, {len(ck['anim_names'])} animations)")
    except Exception as e:
        print(f"[Server] Could not load checkpoint {ckpt_path}: {e}")


# ─────────────────────────────────────────────────────────────────────────────
# 10.  Pydantic request models
# ─────────────────────────────────────────────────────────────────────────────

class RigDef(BaseModel):
    type: str = "ue_mannequin"
    bones: List[str]
    rotation_order: str = "pitch_yaw_roll"
    rotation_unit: str = "degrees"

class PoseDef(BaseModel):
    name: str
    bones: Dict[str, List[float]]

class AnimDef(BaseModel):
    name: str
    fps: float = 30.0
    loop: bool = True
    frame_count: int = 0
    bones: Dict[str, List[List[float]]]

class TrainRequest(BaseModel):
    model_name: str
    rig: RigDef
    poses: List[PoseDef]    = []
    animations: List[AnimDef] = []
    epochs: int             = 50
    learning_rate: float    = 0.001

class GenerateRequest(BaseModel):
    prompt: str
    duration: float = 4.0
    fps: float      = 30.0
    steps: int      = 50
    loop: bool      = True


# ─────────────────────────────────────────────────────────────────────────────
# 11.  Routes
# ─────────────────────────────────────────────────────────────────────────────

@app.post("/train")
async def route_train(req: TrainRequest):
    with _lock:
        if _train_st["status"] == "training":
            raise HTTPException(409, "Training already in progress")
        _train_st.update(status="training", step=0, total=req.epochs,
                         log="Preparing…", error="", should_stop=False)

    bones   = req.rig.bones
    n_bones = len(bones)
    _db.update(bones=bones, n_bones=n_bones)

    records: List[AnimRecord] = []

    for pose in req.poses:
        frame = np.zeros(n_bones * 3, np.float32)
        for bi, bone in enumerate(bones):
            if bone in pose.bones:
                frame[bi*3: bi*3+3] = pose.bones[bone][:3]
        frames = np.tile(frame[np.newaxis], (30, 1))       # 30-frame hold
        records.append(AnimRecord(pose.name, 30, False, frames))

    for anim in req.animations:
        fc  = max(anim.frame_count, 1)
        arr = np.zeros((fc, n_bones * 3), np.float32)
        for bi, bone in enumerate(bones):
            if bone in anim.bones:
                for t, v in enumerate(anim.bones[bone][:fc]):
                    arr[t, bi*3: bi*3+3] = v[:3]
        records.append(AnimRecord(anim.name, anim.fps, anim.loop, arr))

    if not records:
        with _lock: _train_st["status"] = "idle"
        raise HTTPException(400, "No training data")

    _db["records"] = records
    threading.Thread(target=_run_training,
                     args=(req.model_name, req.epochs, req.learning_rate),
                     daemon=True).start()
    return {"status": "started", "samples": len(records)}


@app.get("/train/status")
async def route_train_status():
    with _lock: s = dict(_train_st)
    return {"status": s["status"], "step": s["step"], "total": s["total"],
            "log": s["log"], "error": s.get("error", "")}


@app.post("/train/stop")
async def route_train_stop():
    with _lock: _train_st["should_stop"] = True
    return {"status": "stopping"}


@app.post("/generate")
async def route_generate(req: GenerateRequest):
    try:
        _gen_st.update(stage="starting", step=0, total=100)
        return _generate(req.prompt, req.duration, req.fps, req.loop)
    except HTTPException: raise
    except Exception as e:
        print(traceback.format_exc())
        raise HTTPException(500, str(e))


@app.get("/progress")
async def route_progress():
    return {"stage": _gen_st["stage"], "step": _gen_st["step"], "total": _gen_st["total"]}


@app.get("/health")
async def route_health():
    return {"status": "ok", "trained": _db["ema_model"] is not None,
            "model": _db.get("model_name"), "samples": len(_db.get("records", []))}


@app.get("/models")
async def route_list_models():
    files = sorted(MODEL_DIR.glob("*.pt"), key=lambda p: p.stat().st_mtime, reverse=True)
    return {
        "models": [p.stem for p in files],
        "active": _db.get("model_name"),
    }


class LoadModelRequest(BaseModel):
    name: str

@app.post("/models/load")
async def route_load_model(req: LoadModelRequest):
    ckpt_path = MODEL_DIR / f"{req.name}.pt"
    if not ckpt_path.exists():
        raise HTTPException(404, f"Model '{req.name}' not found")
    try:
        ck  = torch.load(ckpt_path, map_location="cpu")
        cfg = ck["model_cfg"]
        model = MDT(
            n_bones  = cfg["n_bones"],
            text_dim = cfg["text_dim"],
            d_model  = cfg.get("d_model", D_MODEL),
            n_heads  = cfg.get("n_heads", N_HEADS),
            n_layers = cfg.get("n_layers", N_LAYERS),
        )
        model.load_state_dict(ck["ema_state"])
        model.eval()
        _db.update(
            bones      = ck["bones"],
            n_bones    = cfg["n_bones"],
            ema_model  = model,
            model_cfg  = cfg,
            norm_mean  = ck["norm_mean"],
            norm_std   = ck["norm_std"],
            text_embs  = ck["text_embs"],
            anim_names = ck["anim_names"],
            model_name = ck.get("model_name", ckpt_path.stem),
        )
        print(f"[Server] Loaded model: {req.name}  ({cfg['n_bones']} bones)")
        return {"status": "ok", "model": req.name}
    except Exception as e:
        raise HTTPException(500, f"Failed to load model: {e}")


# ─────────────────────────────────────────────────────────────────────────────
# 12.  Entry point
# ─────────────────────────────────────────────────────────────────────────────

if __name__ == "__main__":
    print("=" * 60)
    print("  Embody Animation Diffusion Server")
    print(f"  http://localhost:{PORT}")
    print("  Model: Motion Diffusion Transformer (MDT)")
    print(f"  d_model={D_MODEL}  layers={N_LAYERS}  diff_steps={DIFF_STEPS}")
    print("=" * 60)
    if torch.cuda.is_available():
        print(f"  GPU: {torch.cuda.get_device_name(0)}")
    else:
        print("  !! WARNING: CUDA not available — training will run on CPU.")
        print("  !! To fix: reinstall PyTorch with your CUDA version:")
        print("  !!   pip install torch --index-url https://download.pytorch.org/whl/cu121")
        print("  !! Run setup.bat again to auto-detect and reinstall.")
    print()
    threading.Thread(target=_get_enc,           daemon=True).start()
    threading.Thread(target=_try_load_latest,   daemon=True).start()
    uvicorn.run(app, host="0.0.0.0", port=PORT, log_level="warning")

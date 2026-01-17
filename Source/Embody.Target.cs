// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class EmbodyTarget : TargetRules
{
    public EmbodyTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V6;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_7;
        ExtraModuleNames.Add("Embody");

        // ✅ Allow overrides in installed engine context
        bOverrideBuildEnvironment = true;
        GlobalDefinitions.Add("ALLOW_UDP_MESSAGING_SHIPPING=1");
    }
}
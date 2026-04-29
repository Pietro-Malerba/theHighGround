using UnrealBuildTool;

public class progettoMalerba : ModuleRules
{
    public progettoMalerba(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
    
        // Aggiunti UMG, Slate e SlateCore per la gestione della UI
        PublicDependencyModuleNames.AddRange(new string[] { 
            "Core", 
            "CoreUObject", 
            "Engine", 
            "InputCore", 
            "EnhancedInput",
            "Paper2D", 
            "UMG"
        });

        PrivateDependencyModuleNames.AddRange(new string[] { 
            "Slate", 
            "SlateCore" 
        });
    }
}

@echo off
rem Copies the scanned (Megascans) rocks, cliffs, plants and ground surfaces Shieldbreak uses out of Epic's free
rem Electric Dreams sample into Content\Megascans and Content\MSPresets, keeping their /Game paths so the references
rem hold. They stay out of git (see .gitignore): get the sample from Fab (Electric Dreams Env, "Create project",
rem UE 5.8) first. SB_ELECTRIC_DREAMS overrides the sample's Content folder.
rem Results: Saved\Logs\GetScanArt.log
setlocal
set "ED=%SB_ELECTRIC_DREAMS%"
if "%ED%"=="" set "ED=%USERPROFILE%\Documents\Unreal Projects\ElectricDreamsEnv\Content"
set "SB=%~dp0..\Content"
set "LOG=%~dp0..\Saved\Logs\GetScanArt.log"
if not exist "%~dp0..\Saved\Logs" mkdir "%~dp0..\Saved\Logs"
if not exist "%ED%\Megascans" (
  echo Electric Dreams not found at %ED% > "%LOG%"
  exit /b 1
)
echo Copying from %ED% > "%LOG%"
for %%F in (MossyForestBoulder MossyForestRock MossyRockFace MossyRocks ForestRockFormation QuarryCliff SmallStonesPack MossyEmbankment ForestRoots) do (
  robocopy "%ED%\Megascans\3D_Assets\%%F" "%SB%\Megascans\3D_Assets\%%F" /E /NFL /NDL /NJH /NP >> "%LOG%"
)
for %%F in (BeechFern Ivy EnglishIvy EuropeanHornbeam Elderberry) do (
  robocopy "%ED%\Megascans\3D_Plants\%%F" "%SB%\Megascans\3D_Plants\%%F" /E /NFL /NDL /NJH /NP >> "%LOG%"
)
for %%F in (MossyGrass JungleGround MossyRockyGround BeachCliff NordicMoss IcelandicQuarryRock RockCliff) do (
  robocopy "%ED%\Megascans\Surfaces\%%F" "%SB%\Megascans\Surfaces\%%F" /E /NFL /NDL /NJH /NP >> "%LOG%"
)
robocopy "%ED%\MSPresets" "%SB%\MSPresets" /E /NFL /NDL /NJH /NP >> "%LOG%"
rem Shared master materials, functions and physical materials the scans reference, plus a few stray textures and meshes.
for %%F in (Custom\CustomMaterials Custom\Materials\Functions PhysicalMaterials Megascans\3D_Assets\ErodedRockyGround Megascans\3D_Assets\HugeNordicCoastalCliff Megascans\3D_Plants\Lichen Megascans\Decals\Leakage Meshes\_GENERATED\RockFormation) do (
  robocopy "%ED%\%%F" "%SB%\%%F" /E /NFL /NDL /NJH /NP >> "%LOG%"
)
rem Textures the shared master materials name as defaults (only those, not the plants they belong to).
robocopy "%ED%\Megascans\3D_Plants\AlexandraPalm" "%SB%\Megascans\3D_Plants\AlexandraPalm" T_*.uasset /NFL /NDL /NJH /NP >> "%LOG%"
robocopy "%ED%\Megascans\3D_Plants\CustomMoss" "%SB%\Megascans\3D_Plants\CustomMoss" T_*.uasset /NFL /NDL /NJH /NP >> "%LOG%"
echo COPYDONE >> "%LOG%"
endlocal

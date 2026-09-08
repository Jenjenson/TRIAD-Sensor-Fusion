# GitHub Free collaboration

This repository uses ordinary Git for code, configuration, documentation, and
small reviewable data. Large project-owned source assets and Unreal packages are
published as immutable, versioned GitHub Release assets. This keeps collaboration
within GitHub Free without consuming the 10 GiB Git LFS allowance on every clone.

The release bundle is not a substitute for Unreal Engine, Cesium for Unreal,
AirSimTriadRuntime, or separately licensed Epic Marketplace/Fab content.
Generated caches, credentials, virtual environments, telemetry, backups, and
local editor state are neither source material nor release assets.

## Maintainer: validate the bundle plan

Use PowerShell 7 from the repository root:

    pwsh -File .\scripts\Build-GitHubFreeReleaseBundles.ps1 -UnrealProject "D:\triad\TRIAD" -PlanOnly

The plan uses
distribution/github-free-release-bundles.json. It includes the repository
unreal/SourceAssets tree and the project-owned Unreal configuration, TRIAD
content, maps, external-actor packages, and project descriptor. It excludes
controlled digital-twin releases, caches, secrets, debug symbols, and unrelated
or separately licensed host-project content. It also excludes the unadmitted
outer-context vegetation candidate and its integration evidence while their
ODbL share-alike, machine-readable-access, and runtime-attribution gates remain
explicitly unsatisfied; local source availability is not release authorization.

## Maintainer: build release files

Build only from a stable, reviewed project state. Put the output outside the
Git checkout:

    pwsh -File .\scripts\Build-GitHubFreeReleaseBundles.ps1 -UnrealProject "D:\triad\TRIAD" -OutputDirectory "D:\TRIAD-Releases\2026.09.05" -Version "2026.09.05.1"

Every tar archive is kept below 1.9 GB. triad-release-manifest.json records each
archive's SHA-256, byte count, payload count, extraction destination, source
commit, and external prerequisites.

Install and authenticate GitHub CLI before uploading:

    winget install --id GitHub.cli
    gh auth login

Verify without changing GitHub:

    pwsh -File .\scripts\Publish-GitHubFreeReleaseBundles.ps1 -BundleDirectory "D:\TRIAD-Releases\2026.09.05"

Create an unpublished draft release:

    pwsh -File .\scripts\Publish-GitHubFreeReleaseBundles.ps1 -BundleDirectory "D:\TRIAD-Releases\2026.09.05" -Publish

Review the draft before publishing it. The publisher deliberately refuses to
replace an existing release or upload an archive that differs from its manifest.
Publish the reviewed draft through GitHub's Releases page. When review is not
required, the initial upload can instead use -Publish -Finalize.

## Collaborator: clone and install

Install Git, Git LFS, PowerShell 7, and GitHub CLI. Then:

    git clone https://github.com/Jenjenson/TRIAD-Sensor-Fusion.git
    cd TRIAD-Sensor-Fusion
    gh auth login
    pwsh -File .\scripts\Install-GitHubFreeReleaseBundles.ps1 -Tag latest -UnrealProjectDestination "D:\TRIAD-Work\TRIAD"

The installer downloads through the authenticated GitHub CLI, verifies every
archive before extraction, rejects unsafe archive paths, and refuses to
overwrite existing project roots by default. It links the repository's
unreal/SourceAssets and TRIADSensorFusion plugin into the new host project so
repository edits remain live without duplicate multi-gigabyte copies.

Install Unreal Engine 5.5, Cesium for Unreal, and the authorized
AirSimTriadRuntime dependency before opening TRIAD.uproject.

## Collaboration rules

- Work on a branch and submit a pull request.
- Pull before editing a binary Unreal asset.
- Coordinate ownership of .uasset and .umap files; they cannot be merged as
  source text.
- Publish a new immutable asset release rather than replacing an old release.
- Never commit tokens, .env files, certificates, caches, Saved, Intermediate,
  DerivedDataCache, or downloaded Marketplace content without an explicit
  redistribution review.
- Keep the release manifest and source commit together so every collaborator
  can reconstruct the same state.

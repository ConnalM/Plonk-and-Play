$ErrorActionPreference = 'Stop'
$root = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$source = Get-Content (Join-Path $root 'firmware\include\pp\history_store.h') -Raw

if ($source -notmatch 'struct CommitWorkspace') { throw 'History metadata commit workspace type is missing' }
if ($source -notmatch 'CommitWorkspace\* commitWorkspace_') { throw 'History metadata commit workspace is not explicitly owned by SlotHistoryStore' }
if ($source -notmatch 'delete commitWorkspace_') { throw 'History metadata commit workspace lifetime is not released by SlotHistoryStore' }
if ($source -notmatch 'new \(std::nothrow\) CommitWorkspace') { throw 'History metadata workspace allocation does not have an explicit failure path' }
if ($source -notmatch 'operationBusy_') { throw 'History metadata commit is missing the existing serialized operation guard' }

$classStart = $source.IndexOf('class SlotHistoryStore')
$appendStart = $source.IndexOf('bool append(const uint8_t* bytes', $classStart)
$appendEnd = $source.IndexOf('bool loadNewest(', $appendStart)
$append = $source.Substring($appendStart, $appendEnd - $appendStart)
if ($append -match 'MetadataState candidate\s*=') { throw 'append still allocates the metadata candidate on the loop-task stack' }
if ($append -notmatch 'ensureCommitWorkspace\(\)') { throw 'append does not acquire the owned metadata workspace' }

$publishStart = $source.IndexOf('bool publishMetadata(')
$publishEnd = $source.IndexOf("`n};", $publishStart)
$publish = $source.Substring($publishStart, $publishEnd - $publishStart)
if ($publish -match 'uint8_t\s+bytes\[EncodedMetadataBytes\]') { throw 'publishMetadata still allocates encoded metadata on the stack' }
if ($publish -match 'uint8_t\s+verify\[EncodedMetadataBytes\]') { throw 'publishMetadata still allocates verification metadata on the stack' }
if ($publish -match 'MetadataState\s+checked\s*\{') { throw 'publishMetadata still allocates checked metadata on the stack' }
if ($publish -notmatch 'workspace->bytes' -or $publish -notmatch 'workspace->verify' -or $publish -notmatch 'workspace->checked') { throw 'publishMetadata does not use the owned workspace buffers' }

Write-Host 'PSF metadata workspace contract: PASS owned_serialized_workspace=1 allocation_failure_path=1 stack_buffers_removed=1'

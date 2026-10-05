from pathlib import Path
import hashlib,json,shutil,subprocess,sys,tempfile
ROOT=Path(__file__).resolve().parents[2]; EVIDENCE=Path(__file__).parent/'evidence'
FILES=['core.h','session_definition.h','race_engine.h','noticeboard.h','browser_interface.h']
def evaluate(root): return subprocess.run([sys.executable,str(Path(__file__).with_name('structural_review.py')),'--root',str(root)],capture_output=True,text=True)
def main():
    with tempfile.TemporaryDirectory(prefix='stage14b-evaluator-') as d:
        tmp=Path(d); dest=tmp/'firmware/include/pp';dest.mkdir(parents=True)
        for name in FILES: shutil.copy2(ROOT/'firmware/include/pp'/name,dest/name)
        path=dest/'race_engine.h'; source=path.read_text(); marker='bool hasLap=false,waitingForTimingOrigin=false';
        if marker not in source: return 1
        path.write_text(source.replace(marker,'bool hasLap=false,waitingForTimingOrigin_CORRUPTED=false',1)); bad=evaluate(tmp)
        detected=bad.returncode!=0 and 'practice_engine' in bad.stdout
    good=evaluate(ROOT); restored=good.returncode==0
    record={'corruption':'practice timing-state member renamed in isolated source copy','corrupted_evaluator':'FAIL' if detected else 'UNEXPECTED PASS','restored_evaluator':'PASS' if restored else 'FAIL','corrupted_output':bad.stdout.strip(),'restored_output':good.stdout.strip()}
    EVIDENCE.mkdir(parents=True,exist_ok=True);(EVIDENCE/'evaluator-integrity.json').write_text(json.dumps(record,indent=2)+'\n')
    print('Stage 14B evaluator integrity PASS: corrupted=FAIL restored=PASS' if detected and restored else 'Stage 14B evaluator integrity FAIL')
    return 0 if detected and restored else 1
if __name__=='__main__': raise SystemExit(main())

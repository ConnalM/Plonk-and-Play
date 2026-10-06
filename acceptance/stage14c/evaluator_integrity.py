from pathlib import Path
import hashlib,json,shutil,subprocess,sys,tempfile
root=Path(__file__).resolve().parents[2];out=Path(__file__).parent/'evidence';out.mkdir(exist_ok=True)
with tempfile.TemporaryDirectory(prefix='stage14c-evaluator-') as d:
 t=Path(d);dest=t/'firmware/include/pp';dest.mkdir(parents=True)
 for n in ['core.h','session_definition.h','race_engine.h','noticeboard.h','browser_interface.h']:shutil.copy2(root/'firmware/include/pp'/n,dest/n)
 p=dest/'core.h';s=p.read_text();p.write_text(s.replace('EnduranceExpired','ExpiredEvent'))
 bad=subprocess.run([sys.executable,str(Path(__file__).with_name('structural_review.py')),'--root',str(t)],capture_output=True,text=True)
 detected=bad.returncode!=0
good=subprocess.run([sys.executable,str(Path(__file__).with_name('structural_review.py')),'--root',str(root)],capture_output=True,text=True)
record={'corruption':'production Endurance state member structurally altered in isolated source','corrupted_evaluator':'FAIL' if detected else 'UNEXPECTED PASS','restored_evaluator':'PASS' if good.returncode==0 else 'FAIL','corrupted_output':bad.stdout.strip(),'restored_output':good.stdout.strip()};(out/'evaluator-integrity.json').write_text(json.dumps(record,indent=2)+'\n');print('Stage 14C evaluator integrity PASS: corrupted=FAIL restored=PASS' if detected and good.returncode==0 else 'Stage 14C evaluator integrity FAIL');sys.exit(0 if detected and good.returncode==0 else 1)

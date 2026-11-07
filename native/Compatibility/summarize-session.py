"""Read-only test evidence summary; input polling is never labelled display FPS."""
from pathlib import Path
import json, sys

def summarize(path):
    rows=[]
    incomplete=0
    for line in path.read_text(errors='replace').splitlines():
        try: rows.append(json.loads(line))
        except ValueError: incomplete+=1
    states=[r for r in rows if r.get('event')=='state']
    results=[r for r in rows if r.get('event')=='native_result']
    plays=[r for r in states if (r.get('state'),r.get('substate'))==(2,13)]
    post=[]
    for start in plays:
        result=next((r for r in results if r['ms']>=start['ms']),None)
        returned=next((r for r in states if result and r['ms']>result['ms'] and r['state']==2 and r['substate'] in [12,14]),None)
        post.append({'startMs':start['ms'],'nativeResult':result,
                     'returnState':returned,'wallElapsedToResultSeconds':round((result['ms']-start['ms'])/1000,3) if result else None})
    version=max([r.get('schedulerVersion',1) for r in rows] or [1])
    return {'session':str(path),'rows':len(rows),'incompleteLines':incomplete,
            'adapterInstalled':any(r.get('event')=='input_adapter_installed' for r in rows),
            'states':states,'playAttempts':post,'lastTelemetry':rows[-1] if rows else None,
            'pulses':max([r.get('pulses',0) for r in rows] or [0]),
            'unsupportedTargetObservations':max([r.get('unsupportedTargets',0) for r in rows] or [0]),
            'schedulerVersion':version,
            'observedTargetHitTransitions':{key:max([r.get(key,0) for r in rows] or [0]) for key in ['observedCool','observedFine','observedSafe','observedSad','observedWorst']},
            'lateWindows':max([r.get('lateWindows',0) for r in rows] or [0]),
            'displayFPSMeasured':False,'audioVideoSyncMeasured':False,
            'fullSongVisualAcceptance':'requires user observation; state alone is insufficient',
            'notes':('V2+ pulses count attempted target generations; observed hit transitions are diagnostic samples, not authoritative results. ' if version>=2 else 'V1 pulses count eligible observations, not unique chart notes. ')+
                    'Elapsed times include loading and pauses. Native result fields and percentage units still require in-game confirmation.'}

if __name__=='__main__':
    paths=[Path(s) for s in sys.argv[1:]]
    print(json.dumps([summarize(p) for p in paths],ensure_ascii=False,indent=2))

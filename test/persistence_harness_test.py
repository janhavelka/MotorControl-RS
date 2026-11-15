"""Strict persistence evidence, finite procedures and no hidden write replay."""
import copy
import importlib.util
import json
import sys
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
spec=importlib.util.spec_from_file_location("bench_persistence",ROOT/"scripts/bench_probe.py")
bench=importlib.util.module_from_spec(spec);spec.loader.exec_module(bench)
WINDOWS=((16,2),(19,3),(23,3),(64,5),(256,2),(0,4),(6,2))
REGS=(16,17,19,20,21,23,24,25,64,65,66,67,68,256,257)

def snapshot(at=100):
    config=[0,1000,0,0,0,0,0,0,0,0,0,0,0,2,4000]
    identity=[0x305,0x29,1,0];motion=[0,0]
    result=dict(endpoint=dict(target=1,address=1,generation=9,baud=115200,data_bits=8,parity=1,stop_bits=1,serial_known=True),
                configuration_generation=2,operation_ids=[3,4,5],config_words=config,identity_words=identity,motion_words=motion,provenance=[])
    offset=0
    for index,(reg,count) in enumerate(WINDOWS):
        words=config[offset:offset+count] if index<5 else identity if index==5 else motion
        if index<5:offset+=count
        raw=bytes([1,3,count*2])+b"".join(v.to_bytes(2,"big") for v in words)
        raw+=bench.wire_crc(raw).to_bytes(2,"little")
        result["provenance"].append([reg,count,raw.hex(),at,at+1,at+2,at+3])
    return result

def context(*,kind="save",failed=False,unconfirmed=False,verified=False,restarted=False):
    before=snapshot();raw=bytes([1,6,0,45,0,66 if kind=="save" else 65]);raw+=bench.wire_crc(raw).to_bytes(2,"little")
    wire=dict(step=0,event=1 if failed else 0,raw_hex="" if failed else raw.hex(),received_length=0 if failed else 8,
              tx_accepted=8,tx_complete=True,response_confirmed=not failed and not unconfirmed,qualified=not failed,
              execution_unknown=failed,earliest_us=0 if failed else 1010,latest_us=0 if failed else 1020,delivered_us=1030,
              status="ILLEGAL_VALUE" if failed else "OK",detail=0,frame_error=0,transport_detail=0)
    result=dict(operation_id=7,kind=kind,state=3 if failed or unconfirmed else 2,outcome=3 if failed else 7 if unconfirmed else 1,
                execution=3 if failed or unconfirmed else 1,persistence=int(restarted),status="ILLEGAL_VALUE" if failed or unconfirmed else "OK",detail=0,
                register=45,value=66 if kind=="save" else 65,effects=True,uncertain=failed or unconfirmed,snapshot_complete=False,
                verification_known=verified,live_readback_known=verified,restart_required_for_proof=not restarted,
                manual_intervention_required=kind=="factory-restore",communication_changed=False,configuration_invalidated=True,
                matching_fields=32767 if verified else 0,verified_fields=32767 if restarted else 0,verification_count=int(verified),
                started_us=1000,deadline_us=20000,backup_source=1,complete_backup=kind=="factory-restore",before=before,
                write_evidence=wire,verification=snapshot(2000) if verified else None,restart_observed=restarted,restart_us=1500 if restarted else 0,
                restart_source=99 if restarted else 0,
                fields=[[reg,value,value if verified else 0,"RW/S" if reg>=256 else "RW",verified,restarted] for reg,value in zip(REGS,before["config_words"])])
    return result

def response(tokens=(),**changes):
    expected=bench.persistence_arguments(tokens);plan=None
    if expected["action"] in (0,1):
        plan={k:v for k,v in expected.items() if k!="action"}
        plan.update(writes=1,scope="all_parameters",stopped_required=True,completion="unresolved",durability="unverified",restart="required_for_proof",backup="partial_standard_config")
    item=dict(type="reply",profile="ess_rs",id=1,command="persistence",ok=True,result="done",action=expected["action"],pending=False,owned=False,
              route_ready=True,snapshot_known=False,snapshot_failed=False,verification_attempts=0,parent_session=False,finished=False,invocations=0,invocation_limit=2,
              restart_performed=False,write_replayed=False,plan=plan,baseline=None,context=None,
              host=dict(active=dict(baud=115200,format="8N1"),active_known=True,blocked=False,serial_generation=1,actual_baud=115200,failure="none"))
    item.update(changes);return item

class Clock:
    def __init__(self):self.now=0
    def __call__(self):return self.now
    def sleep(self,t):self.now+=t

class Port:
    def __init__(self,factory):self.factory=factory;self.data=b"";self.sent=[]
    def write(self,data):
        self.sent.append(data);tokens=data.decode().strip().split();item=self.factory(tuple(tokens[2:]));item["id"]=int(tokens[0][1:])
        self.data+=json.dumps(item).encode()+b"\n";return len(data)
    def read(self,n):result,self.data=self.data[:n],self.data[n:];return result

class Campaign:
    def __init__(self,fail=None):self.clock=Clock();self.sleep=self.clock.sleep;self.fail=fail;self.calls=[];self.events=[];self.emit=lambda *a,**k:self.events.append((a,k))
    def command(self,name,*,host_args,timeout_s):
        assert name=="persistence";self.calls.append(host_args)
        r=response(host_args)
        action=host_args[0]
        if action=="snapshot":r.update(snapshot_known=True,baseline=snapshot(),owned=True)
        if action in ("begin","inspect","verify","finish"):
            c=context(failed=self.fail=="lost_ack",verified=action in ("verify","finish"))
            r.update(context=c,owned=action!="finish",snapshot_known=True,invocations=1,finished=action=="finish")
        if self.fail=="pending" and action in ("begin","inspect"):r["pending"]=True
        if self.fail==action:r.update(ok=False,result="failed")
        return r

# Exact inspection replies emitted by the real application/owner/runner with UART
# fakes at 4a5d09d. Calls use the existing persistence_app_test.cpp helpers.
# Cases: save rejected03; save exception05; pending save. Each starts with
# baseline(), begin(), then reply(crc({1,0x86,code})) where applicable.
APPLICATION_INSPECTIONS = tuple(json.loads(line) for line in r'''
{"type":"reply","profile":"ess_rs","id":999,"command":"persistence","ok":true,"result":"done","action":-1,"pending":false,"owned":true,"route_ready":true,"snapshot_known":false,"snapshot_failed":false,"verification_attempts":0,"parent_session":false,"finished":false,"invocations":1,"invocation_limit":2,"restart_performed":false,"write_replayed":false,"plan":null,"baseline":null,"context":{"operation_id":4,"kind":"save","state":3,"outcome":2,"execution":2,"persistence":0,"status":"EXCEPTION","detail":3,"register":45,"value":66,"effects":true,"uncertain":false,"snapshot_complete":false,"verification_known":false,"live_readback_known":false,"restart_required_for_proof":true,"manual_intervention_required":false,"communication_changed":false,"configuration_invalidated":true,"matching_fields":0,"verified_fields":0,"verification_count":0,"started_us":477052,"deadline_us":977052,"backup_source":77,"complete_backup":false,"before":{"endpoint":{"target":1,"address":1,"generation":1,"baud":115200,"data_bits":8,"parity":1,"stop_bits":1,"serial_known":true} ,"configuration_generation":1,"operation_ids":[2,1,3],"config_words":[0,400,0,0,0,1,0,0,0,0,0,0,0,0,4000],"identity_words":[20202,41,1,0],"motion_words":[0,1],"provenance":[[16,2,"01030400000190FBCF",8470,14530,14554,14591],[19,3,"0103060000000000002175",14591,21812,21840,21865],[23,3,"0103060001000000001CB5",21865,29086,29114,29139],[64,5,"01030A0000000000000000000024B6",29139,36712,36748,36757],[256,2,"01030400000FA0FFBB",36757,43810,43834,43871],[0,4,"0103084EEA00290001000057A3",1002,7398,7430,7463],[6,2,"010304000000013BF3",250210,256264,256288,256325]]},"write_evidence":{"step":0,"event":0,"raw_hex":"0186030261","received_length":5,"tx_accepted":8,"tx_complete":true,"response_confirmed":true,"qualified":true,"execution_unknown":false,"earliest_us":482746,"latest_us":482782,"delivered_us":482789,"transport_detail":1,"status":"EXCEPTION","detail":3,"frame_error":10},"verification":null,"restart_observed":false,"restart_us":0,"restart_source":0,"fields":[[16,0,0,"RW",false,false],[17,400,0,"RW",false,false],[19,0,0,"RW",false,false],[20,0,0,"RW",false,false],[21,0,0,"RW",false,false],[23,1,0,"RW",false,false],[24,0,0,"RW",false,false],[25,0,0,"RW",false,false],[64,0,0,"RW",false,false],[65,0,0,"RW",false,false],[66,0,0,"RW",false,false],[67,0,0,"RW",false,false],[68,0,0,"RW",false,false],[256,0,0,"RW/S",false,false],[257,4000,0,"RW/S",false,false]]},"host":{"active":{"baud":115200,"format":"8N1"},"active_known":true,"blocked":false,"serial_generation":1,"actual_baud":115211,"failure":"none"}}
{"type":"reply","profile":"ess_rs","id":999,"command":"persistence","ok":true,"result":"done","action":-1,"pending":false,"owned":true,"route_ready":true,"snapshot_known":false,"snapshot_failed":false,"verification_attempts":0,"parent_session":false,"finished":false,"invocations":1,"invocation_limit":2,"restart_performed":false,"write_replayed":false,"plan":null,"baseline":null,"context":{"operation_id":4,"kind":"save","state":3,"outcome":2,"execution":3,"persistence":0,"status":"EXCEPTION","detail":5,"register":45,"value":66,"effects":true,"uncertain":true,"snapshot_complete":false,"verification_known":false,"live_readback_known":false,"restart_required_for_proof":true,"manual_intervention_required":false,"communication_changed":false,"configuration_invalidated":true,"matching_fields":0,"verified_fields":0,"verification_count":0,"started_us":477052,"deadline_us":977052,"backup_source":77,"complete_backup":false,"before":{"endpoint":{"target":1,"address":1,"generation":1,"baud":115200,"data_bits":8,"parity":1,"stop_bits":1,"serial_known":true} ,"configuration_generation":1,"operation_ids":[2,1,3],"config_words":[0,400,0,0,0,1,0,0,0,0,0,0,0,0,4000],"identity_words":[20202,41,1,0],"motion_words":[0,1],"provenance":[[16,2,"01030400000190FBCF",8470,14530,14554,14591],[19,3,"0103060000000000002175",14591,21812,21840,21865],[23,3,"0103060001000000001CB5",21865,29086,29114,29139],[64,5,"01030A0000000000000000000024B6",29139,36712,36748,36757],[256,2,"01030400000FA0FFBB",36757,43810,43834,43871],[0,4,"0103084EEA00290001000057A3",1002,7398,7430,7463],[6,2,"010304000000013BF3",250210,256264,256288,256325]]},"write_evidence":{"step":0,"event":0,"raw_hex":"0186058263","received_length":5,"tx_accepted":8,"tx_complete":true,"response_confirmed":true,"qualified":true,"execution_unknown":false,"earliest_us":482746,"latest_us":482782,"delivered_us":482789,"transport_detail":1,"status":"EXCEPTION","detail":5,"frame_error":10},"verification":null,"restart_observed":false,"restart_us":0,"restart_source":0,"fields":[[16,0,0,"RW",false,false],[17,400,0,"RW",false,false],[19,0,0,"RW",false,false],[20,0,0,"RW",false,false],[21,0,0,"RW",false,false],[23,1,0,"RW",false,false],[24,0,0,"RW",false,false],[25,0,0,"RW",false,false],[64,0,0,"RW",false,false],[65,0,0,"RW",false,false],[66,0,0,"RW",false,false],[67,0,0,"RW",false,false],[68,0,0,"RW",false,false],[256,0,0,"RW/S",false,false],[257,4000,0,"RW/S",false,false]]},"host":{"active":{"baud":115200,"format":"8N1"},"active_known":true,"blocked":false,"serial_generation":1,"actual_baud":115211,"failure":"none"}}
{"type":"reply","profile":"ess_rs","id":999,"command":"persistence","ok":true,"result":"done","action":-1,"pending":true,"owned":true,"route_ready":true,"snapshot_known":false,"snapshot_failed":false,"verification_attempts":0,"parent_session":false,"finished":false,"invocations":1,"invocation_limit":2,"restart_performed":false,"write_replayed":false,"plan":null,"baseline":null,"context":{"operation_id":4,"kind":"save","state":1,"outcome":0,"execution":0,"persistence":0,"status":"OK","detail":0,"register":45,"value":66,"effects":false,"uncertain":false,"snapshot_complete":false,"verification_known":false,"live_readback_known":false,"restart_required_for_proof":true,"manual_intervention_required":false,"communication_changed":false,"configuration_invalidated":false,"matching_fields":0,"verified_fields":0,"verification_count":0,"started_us":477052,"deadline_us":977052,"backup_source":77,"complete_backup":false,"before":{"endpoint":{"target":1,"address":1,"generation":1,"baud":115200,"data_bits":8,"parity":1,"stop_bits":1,"serial_known":true} ,"configuration_generation":1,"operation_ids":[2,1,3],"config_words":[0,400,0,0,0,1,0,0,0,0,0,0,0,0,4000],"identity_words":[20202,41,1,0],"motion_words":[0,1],"provenance":[[16,2,"01030400000190FBCF",8470,14530,14554,14591],[19,3,"0103060000000000002175",14591,21812,21840,21865],[23,3,"0103060001000000001CB5",21865,29086,29114,29139],[64,5,"01030A0000000000000000000024B6",29139,36712,36748,36757],[256,2,"01030400000FA0FFBB",36757,43810,43834,43871],[0,4,"0103084EEA00290001000057A3",1002,7398,7430,7463],[6,2,"010304000000013BF3",250210,256264,256288,256325]]},"write_evidence":{"step":0,"event":0,"raw_hex":"","received_length":0,"tx_accepted":0,"tx_complete":false,"response_confirmed":false,"qualified":false,"execution_unknown":false,"earliest_us":0,"latest_us":0,"delivered_us":0,"transport_detail":0,"status":"OK","detail":0,"frame_error":0},"verification":null,"restart_observed":false,"restart_us":0,"restart_source":0,"fields":[[16,0,0,"RW",false,false],[17,400,0,"RW",false,false],[19,0,0,"RW",false,false],[20,0,0,"RW",false,false],[21,0,0,"RW",false,false],[23,1,0,"RW",false,false],[24,0,0,"RW",false,false],[25,0,0,"RW",false,false],[64,0,0,"RW",false,false],[65,0,0,"RW",false,false],[66,0,0,"RW",false,false],[67,0,0,"RW",false,false],[68,0,0,"RW",false,false],[256,0,0,"RW/S",false,false],[257,4000,0,"RW/S",false,false]]},"host":{"active":{"baud":115200,"format":"8N1"},"active_known":true,"blocked":false,"serial_generation":1,"actual_baud":115211,"failure":"none"}}
'''.strip().splitlines())


class PersistenceTests(unittest.TestCase):
    def console(self,factory=response):
        clock=Clock();port=Port(factory);c=bench.Console(port,clock=clock,sleeper=clock.sleep);c.identified=True;return c,port
    def emitted(self, index, mutate=None):
        item = copy.deepcopy(APPLICATION_INSPECTIONS[index])
        if mutate:
            mutate(item["context"])
        c, _ = self.console(lambda tokens: item)
        return c.command("persistence", host_args=("inspect",), timeout_s=.1)
    def test_actual_application_rejection_unknown_and_pending(self):
        for index, original in enumerate(APPLICATION_INSPECTIONS):
            with self.subTest(index=index):
                self.assertEqual(self.emitted(index)["context"], original["context"])
    def test_actual_unknown_write_cannot_be_relabelled(self):
        for execution in (0, 1, 2):
            with self.subTest(execution=execution), self.assertRaises(bench.BenchError):
                self.emitted(1, lambda c: c.update(execution=execution, uncertain=False))
        with self.assertRaises(bench.BenchError):
            self.emitted(0, lambda c: c.update(execution=3, uncertain=True))
    def test_actual_write_closure_and_raw_status_determine_execution(self):
        def boundary(c, late):
            c["deadline_us"] = c["write_evidence"]["latest_us"] - int(late)
            c["write_evidence"]["delivered_us"] += 1000000
        self.assertEqual(self.emitted(0, lambda c: boundary(c, False))["context"]["execution"], 2)
        mutations = (
            lambda c: boundary(c, True),
            lambda c: c["write_evidence"].update(execution_unknown=True),
            lambda c: c["write_evidence"].update(detail=5),
            lambda c: c["write_evidence"].update(raw_hex="0186030000"),
        )
        for mutate in mutations:
            with self.subTest(mutate=mutate), self.assertRaises(bench.BenchError):
                self.emitted(0, mutate)
    def test_strict_grammar_has_no_write_escape_or_guessed_candidate(self):
        for tokens in (("begin",),("begin","all"),("begin","save","1"),("save",),("host","requested"),("verify","extra")):
            with self.subTest(tokens=tokens),self.assertRaises(ValueError):bench.persistence_arguments(tokens)
        self.assertEqual(bench.persistence_arguments(("plan","save"))["value"],66)
        self.assertEqual(bench.persistence_arguments(("begin","factory-restore"))["value"],65)
        self.assertEqual(bench.persistence_arguments(("host","before"))["action"],5)
    def test_real_wire_is_profile_route_and_inspect_does_not_create_motor_operation(self):
        c,p=self.console();c.command("persistence",host_args=("plan","save"),timeout_s=.1)
        self.assertEqual(p.sent,[b"@1 persistence plan save\n"]);self.assertFalse(c.operations)
    def test_unavailable_begin_retains_diagnostic_reply_without_retry(self):
        c,p=self.console(lambda t:response(t,ok=False,result="unavailable",route_ready=False))
        self.assertFalse(c.command("persistence",host_args=("begin","save"),timeout_s=.1)["ok"]);self.assertEqual(len(p.sent),1)
    def test_invalid_plan_and_false_source_claims_reject(self):
        for mutate in (lambda r:r.update(action=5),lambda r:r["plan"].update(value=65),lambda r:r["plan"].update(durability="verified"),lambda r:r.update(write_replayed=True),lambda r:r.update(invocations=3)):
            def factory(t):r=response(t);mutate(r);return r
            c,p=self.console(factory)
            with self.assertRaises(bench.BenchError):c.command("persistence",host_args=("plan","save"),timeout_s=.1)
            self.assertEqual(len(p.sent),1)
    def test_checked_ack_and_live_readback_never_infer_durability(self):
        retained=context(verified=True)
        c,p=self.console(lambda t:response(t,owned=True,context=retained,snapshot_known=True,invocations=1))
        result=c.command("persistence",host_args=("inspect",),timeout_s=.1)
        self.assertEqual(result["context"]["verified_fields"],0);self.assertTrue(result["context"]["restart_required_for_proof"])
    def test_lost_ack_remains_uncertain_after_verified_readback(self):
        c,p=self.console(lambda t:response(t,owned=True,context=context(failed=True,verified=True),snapshot_known=True,invocations=1))
        result=c.command("persistence",host_args=("inspect",),timeout_s=.1)
        self.assertTrue(result["context"]["uncertain"]);self.assertEqual(result["context"]["execution"],3)
    def test_source_unconfirmed_echo_has_unknown_execution(self):
        c,p=self.console(lambda t:response(t,owned=True,context=context(unconfirmed=True),snapshot_known=True,invocations=1))
        self.assertEqual(c.command("persistence",host_args=("inspect",),timeout_s=.1)["context"]["execution"],3)
    def test_only_explicit_actual_restart_can_prove_matching_individual_fields(self):
        c,p=self.console(lambda t:response(t,owned=True,context=context(verified=True,restarted=True),snapshot_known=True,invocations=1))
        self.assertEqual(c.command("persistence",host_args=("inspect",),timeout_s=.1)["context"]["verified_fields"],32767)
    def test_fabricated_proof_and_raw_readback_are_rejected(self):
        mutations=(lambda r:r.update(snapshot_complete=True),lambda r:r.update(verified_fields=1),lambda r:r["write_evidence"].update(response_confirmed=False),
                   lambda r:r["before"]["provenance"][0].__setitem__(2,"010304000000000000"),lambda r:r["fields"][0].__setitem__(2,5),
                   lambda r:r.update(restart_source=0),lambda r:r.update(kind="factory-restore",value=65))
        for mutate in mutations:
            retained=context(verified=True,restarted=True);mutate(retained)
            c,p=self.console(lambda t:response(t,owned=True,context=retained,snapshot_known=True,invocations=1))
            with self.subTest(retained=retained),self.assertRaises(bench.BenchError):c.command("persistence",host_args=("inspect",),timeout_s=.1)
    def test_inspect_cannot_replace_session_or_write_evidence(self):
        retained=context();c,p=self.console(lambda t:response(t,owned=True,context=copy.deepcopy(retained),snapshot_known=True,invocations=1))
        c.command("persistence",host_args=("inspect",),timeout_s=.1);retained["write_evidence"]["delivered_us"]+=1
        with self.assertRaises(bench.BenchError):c.command("persistence",host_args=("inspect",),timeout_s=.1)
    def test_snapshot_raw_evidence_can_precede_context(self):
        c,p=self.console(lambda t:response(t,owned=True,snapshot_known=True,baseline=snapshot()))
        self.assertTrue(c.command("persistence",host_args=("snapshot",),timeout_s=.1)["snapshot_known"])
    def test_diagnostic_snapshot_can_report_moving_alarm_or_unknown_state(self):
        for alarm, motion in ((0, 4), (1, 0), (0, 0x8000)):
            baseline = snapshot()
            baseline["motion_words"] = [alarm, motion]
            raw = bytes([1, 3, 4]) + alarm.to_bytes(2, "big") + motion.to_bytes(2, "big")
            baseline["provenance"][-1][2] = (raw + bench.wire_crc(raw).to_bytes(2, "little")).hex()
            c, _ = self.console(lambda t: response(t, owned=True, snapshot_known=True, baseline=baseline))
            with self.subTest(alarm=alarm, motion=motion):
                self.assertEqual(c.command("persistence", host_args=("snapshot",), timeout_s=.1)["baseline"]["motion_words"], [alarm, motion])
                retained = context()
                retained["before"] = baseline
                c, _ = self.console(lambda t: response(t, owned=True, context=retained, invocations=1))
                with self.assertRaises(bench.BenchError):
                    c.command("persistence", host_args=("inspect",), timeout_s=.1)
    def test_parent_finish_keeps_communication_lease(self):
        c,p=self.console(lambda t:response(t,owned=True,parent_session=True,finished=True,context=context(verified=True),snapshot_known=True,invocations=1))
        self.assertTrue(c.command("persistence",host_args=("finish",),timeout_s=.1)["owned"])
    def test_read_only_snapshot_finish_needs_no_invocation_context(self):
        c,p=self.console(lambda t:response(t,finished=True,snapshot_known=True,baseline=snapshot()))
        self.assertIsNone(c.command("persistence",host_args=("finish",),timeout_s=.1)["context"])
    def test_new_baseline_visible_with_historical_context(self):
        retained=context(verified=True);new=snapshot(3000);new["operation_ids"]=[8,9,10]
        c,p=self.console(lambda t:response(t,owned=True,finished=True,snapshot_known=True,baseline=new,context=retained,invocations=1))
        r=c.command("persistence",host_args=("inspect",),timeout_s=.1)
        self.assertEqual(r["baseline"]["operation_ids"],[8,9,10]);self.assertEqual(r["context"]["before"]["operation_ids"],[3,4,5])
    def test_host_before_uses_new_baseline_when_old_invocation_finished(self):
        next=snapshot(3000);next["endpoint"]["baud"]=38400
        r=response(("host","before"),owned=True,finished=True,snapshot_known=True,baseline=next,context=context(verified=True),invocations=1)
        r["host"]["active"]["baud"]=38400
        c,p=self.console(lambda t:r)
        self.assertTrue(c.command("persistence",host_args=("host","before"),timeout_s=.1)["ok"])
    def test_failed_verification_attempt_and_invocation_budgets_remain_visible(self):
        r=response(("inspect",),owned=True,context=context(),invocations=1,snapshot_failed=True,verification_attempts=2)
        c,p=self.console(lambda t:copy.deepcopy(r));first=c.command("persistence",host_args=("inspect",),timeout_s=.1)
        self.assertTrue(first["snapshot_failed"]);self.assertEqual(first["verification_attempts"],2)
        r["invocations"]=0
        with self.assertRaises(bench.BenchError):c.command("persistence",host_args=("inspect",),timeout_s=.1)
    def test_finish_without_post_action_snapshot_rejects(self):
        c,p=self.console(lambda t:response(t,finished=True,context=context(),invocations=1))
        with self.assertRaises(bench.BenchError):c.command("persistence",host_args=("finish",),timeout_s=.1)
    def test_preview_sends_no_snapshot_or_write(self):
        c=Campaign();bench.persistence_campaign(c,kind="save",timeout_s=1)
        self.assertEqual(c.calls,[("plan","save")])
    def test_explicit_campaign_has_one_write_and_no_recovery(self):
        c=Campaign();bench.persistence_campaign(c,kind="save",timeout_s=1,execute=True,verify=True,finish=True)
        self.assertEqual(c.calls,[("plan","save"),("snapshot",),("begin","save"),("verify",),("finish",)])
    def test_failed_lost_and_pending_write_never_replay_or_cleanup(self):
        for fault in ("begin","lost_ack","pending"):
            c=Campaign(fault)
            with self.subTest(fault=fault),self.assertRaises(bench.BenchError):bench.persistence_campaign(c,kind="save",timeout_s=2,execute=True,verify=True,finish=True)
            self.assertEqual(c.calls.count(("begin","save")),1);self.assertNotIn(("verify",),c.calls);self.assertNotIn(("finish",),c.calls)
            self.assertLessEqual(c.calls.count(("inspect",)),100)
    def test_bad_campaign_options_fail_before_console(self):
        c=Campaign()
        with self.assertRaises(ValueError):bench.persistence_campaign(c,kind="save",timeout_s=1,verify=True)
        self.assertFalse(c.calls)

if __name__=="__main__":unittest.main()

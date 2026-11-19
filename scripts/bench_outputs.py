"""Wired bench 06: drive 4 Y0 output functions against aux1 and serial state.

One case per run (O1-O11, prompt 06 case table). Each run reads I/O and
state, sets Y0 through the ordinary I/O route with stated facts (load
indicator, levels either), runs the case's stimulus, collects aux1 edges and
debug traffic and tabulates them through the analysis below, then restores
through the run's restoration plan. aux1 is read inverted: Y0 on pulls it
inactive. No bench output is switched here.

The analysis is controller-clock intervals (COMMON Timing): an edge against
the request start and reply end of the write that caused it, and the drive's
own transition bracket [request start of the last sample with the old value,
reply end of the first sample with the new value] relative to the edge.
Overlap or a reversed order is a measured relationship, never a fault.
"""
import statistics
import time

import bench_probe as probe
import bench_records as records

OWNER = records.FIXTURE['y0_owner']
Y0_FACTS = ['indicator', 'either']
MOTION_REGISTER, AUXILIARY_REGISTER = 0x27, 0x2D
RUNNING, IN_POSITION, HOMED, RELEASED, ALARM = 4, 1, 2, 16, 8
START_RELATIVE, STOP_FAST = 1, 0x200
RELEASE, ENABLE, CLEAR_ALARM, CLEAR_POSITION = 17, 18, 33, 49

# Case table (prompt 06). function: Y0 function; polarity: 0x004B bit 0 for
# the stimulus; repetitions of the timing rows.
CASES = {
    'O1': {'function': 9, 'polarity': 0, 'kind': 'custom', 'repetitions': 10,
           'levels': ['controller-timestamped', 'read back']},
    'O2': {'function': 9, 'polarity': 1, 'kind': 'custom', 'repetitions': 10,
           'levels': ['controller-timestamped', 'read back']},
    'O3': {'function': 0, 'polarity': None, 'kind': 'levels', 'repetitions': 0, 'levels': ['controller-timestamped']},
    'O4': {'function': 2, 'polarity': 0, 'kind': 'running', 'repetitions': 10,
           'levels': ['controller-timestamped', 'drive-reported']},
    'O5': {'function': 2, 'polarity': 1, 'kind': 'running', 'repetitions': 1, 'levels': ['controller-timestamped']},
    'O6': {'function': 4, 'polarity': 0, 'kind': 'arrival', 'repetitions': 10,
           'levels': ['controller-timestamped', 'drive-reported']},
    'O7': {'function': 3, 'polarity': 0, 'kind': 'homing', 'repetitions': 10,
           'levels': ['controller-timestamped', 'drive-reported']},
    'O8': {'function': 5, 'polarity': 0, 'kind': 'brake', 'repetitions': 10,
           'levels': ['controller-timestamped', 'drive-reported']},
    'O9': {'function': 1, 'polarity': 0, 'kind': 'alarm', 'repetitions': 3,
           'levels': ['controller-timestamped', 'drive-reported']},
    # D9 experiment cases: run on image A (the experiment admission was removed
    # with the ledger record, Implement 3); later images refuse them before the port opens.
    'O10': {'function': 10, 'polarity': 0, 'kind': 'experiment', 'custom': [1, 2, 3, 0], 'repetitions': 1,
            'levels': ['controller-timestamped', 'read back'], 'removed': True},
    'O11': {'function': 11, 'polarity': 0, 'kind': 'experiment', 'custom': [1, 2, 3, 4, 0], 'repetitions': 1,
            'levels': ['controller-timestamped', 'read back'], 'removed': True},
}
MOVE_STEPS = (1000, -1000, 3000, -3000)

# --------------------------------------------------------------------------
# Analysis (pure; unit-tested with recorded replies)


def state_sample(reply: dict) -> dict | None:
    """One `read state` reply as a sample: motion word, alarm, I/O status and
    the controller-clock window of each read step (request start bound to
    reply end bound)."""
    if not isinstance(reply, dict) or not reply.get('ok'):
        return None
    try:
        blocks = {b['block']: b for b in reply['state_blocks']}
        steps = {s['first']: s for s in reply['steps']}
        motion, status, feedback = steps[6], steps[8], steps[10]
        return {'operation_id': reply.get('operation_id'), 'address': reply.get('address'),
                'motion': blocks[0]['raw_motion'], 'alarm': blocks[0]['raw_alarm'],
                'outputs': blocks[1]['raw_outputs'], 'inputs': blocks[1]['raw_inputs'],
                'speed': blocks[2]['raw_speed'], 'position_words': blocks[2].get('position_words'),
                'windows': {'motion': [motion['attempted_us'], motion['latest_us']],
                            'outputs': [status['attempted_us'], status['latest_us']],
                            'feedback': [feedback['attempted_us'], feedback['latest_us']]}}
    except (KeyError, TypeError):
        return None


def sample_value(sample: dict, key: tuple) -> int:
    """Value of one drive quantity: ('motion', bit), ('outputs', bit) or ('alarm',)."""
    if key[0] == 'alarm':
        return int(bool(sample['alarm']) or bool(sample['motion'] & ALARM))
    return int(bool(sample[key[0]] & key[1]))


def _window(sample: dict, key: tuple) -> list:
    return sample['windows']['outputs' if key[0] == 'outputs' else 'motion']


def transitions(samples: list, key: tuple) -> list:
    """Consecutive samples whose value differs. The drive's transition lies in
    [request start of the old sample, reply end of the new one]."""
    ordered = sorted((s for s in samples if s), key=lambda s: _window(s, key)[0])
    out = []
    for old, new in zip(ordered, ordered[1:]):
        a, b = sample_value(old, key), sample_value(new, key)
        if a != b:
            out.append({'from': a, 'to': b, 'earliest_us': _window(old, key)[0], 'latest_us': _window(new, key)[1],
                        'old_operation_id': old['operation_id'], 'new_operation_id': new['operation_id']})
    return out


def bracket(edge_us: int, found: list, new_value: int, limit_us: int = 3000000) -> dict:
    """The transition to ``new_value`` nearest the edge, relative to the edge."""
    candidates = [t for t in found if t['to'] == new_value]
    if not candidates:
        return {'status': 'not observed', 'reason': 'no sample pair shows the transition'}

    def distance(t):
        return 0 if t['earliest_us'] <= edge_us <= t['latest_us'] else \
            min(abs(edge_us - t['earliest_us']), abs(edge_us - t['latest_us']))
    nearest = min(candidates, key=distance)
    if distance(nearest) > limit_us:
        return {'status': 'not observed', 'reason': 'no transition within %d us of the edge' % limit_us}
    relative = [nearest['earliest_us'] - edge_us, nearest['latest_us'] - edge_us]
    relation = 'contains the edge' if relative[0] <= 0 <= relative[1] else \
        'before the edge' if relative[1] < 0 else 'after the edge'
    return {'status': 'measured', 'relative_us': relative, 'relation': relation,
            'old_operation_id': nearest['old_operation_id'], 'new_operation_id': nearest['new_operation_id']}


def classify_write(record: dict, owner: int = OWNER):
    """One TX traffic record as (function, register, value) of the owner, or None."""
    try:
        raw = bytes.fromhex(record.get('raw_hex', ''))
    except ValueError:
        return None
    if record.get('kind') != 'TX' or len(raw) < 8 or raw[0] != owner or raw[1] not in (6, 16):
        return None
    register = int.from_bytes(raw[2:4], 'big')
    value = int.from_bytes(raw[4:6], 'big') if raw[1] == 6 else None
    return {'function': raw[1], 'register': register, 'value': value}


def wire_writes(traffic: list, owner: int = OWNER) -> list:
    """Every owner write in the traffic: TX request start, the reply's end and its uncertainty."""
    by_transaction = {}
    for record in sorted((r for r in traffic if isinstance(r, dict) and type(r.get('sequence')) is int),
                         key=lambda r: r['sequence']):
        by_transaction.setdefault(record.get('transaction'), []).append(record)
    out = []
    for transaction, items in by_transaction.items():
        tx = next((r for r in items if classify_write(r, owner)), None)
        if tx is None:
            continue
        rx = next((r for r in items if r.get('kind') == 'RX' and r['sequence'] > tx['sequence']), None)
        out.append({'transaction': transaction, **classify_write(tx, owner), 'tx_start_us': tx['at_us'],
                    'ack_end_us': rx['end_us'] if rx else None,
                    'uncertainty_us': rx.get('uncertainty_us', 0) if rx else None})
    return sorted(out, key=lambda w: w['tx_start_us'])


def find_write(writes: list, register: int, value, after_us: int, before_us: int | None = None) -> dict | None:
    """First wire write to ``register`` (and ``value`` for FC06) in [after_us, before_us)."""
    for write in writes:
        if write['register'] != register or write['tx_start_us'] < after_us:
            continue
        if before_us is not None and write['tx_start_us'] >= before_us:
            break
        if value is None or write['value'] == value:
            return write
    return None


def closure(reply: dict, register: int | None = None) -> dict | None:
    """Reply closure bounds of the write step of one operation result (the
    operation's own evidence, independent of the lossy debug copy): a move's
    trigger, an action's write, or the FC06 row of a settings result. Closure
    is the reply end plus the RTU inter-frame silence, not the last byte."""
    if not isinstance(reply, dict):
        return None
    for key in ('trigger_evidence', 'write_evidence'):
        e = reply.get(key)
        if isinstance(e, dict) and e.get('qualified'):
            return {'earliest_us': e['earliest_us'], 'latest_us': e['latest_us']}
    columns, rows = reply.get('evidence_columns'), reply.get('evidence')
    if isinstance(columns, list) and isinstance(rows, list):
        for row in rows:
            r = dict(zip(columns, row))
            if r.get('write') and (register is None or r.get('register') == register) and r.get('qualified'):
                return {'earliest_us': r['earliest_us'], 'latest_us': r['latest_us']}
    return None


def stats(values: list) -> dict | None:
    values = [v for v in values if v is not None]
    if not values:
        return None
    return {'n': len(values), 'min': min(values), 'median': statistics.median(values), 'max': max(values)}


def analyse(rows: list, edges: list, samples: list, writes: list, lost: list) -> dict:
    """Assign each aux1 edge to its row, check count and direction, and
    tabulate the intervals.

    A row: {label, start_us, end_us, expect}. ``expect`` lists the edges the
    row must produce, in order: {'active': aux1 level after the edge, 'write':
    {'register', 'value'} of the write that causes it or None, 'key': drive
    quantity or None, 'new': the drive value after it}. ``expect`` None only
    records the edges of the row (status ``recorded``). Edges in
    [start_us, end_us) belong to the row; edges in no row are unattributed. A
    lost-edge window over a row makes its count indeterminate.
    """
    out_rows, attributed = [], set()
    for row in rows:
        mine = [e for e in edges if row['start_us'] <= e['at_us'] < row['end_us']]
        attributed.update(e['at_us'] for e in mine)
        expect = row.get('expect')
        result = {'label': row['label'], 'start_us': row['start_us'], 'end_us': row['end_us'], 'edges': mine,
                  'expected': None if expect is None else len(expect), 'intervals': []}
        overwritten = any(w['from_us'] is not None and w['to_us'] is not None and w['from_us'] < row['end_us'] and
                          w['to_us'] > row['start_us'] for w in lost)
        if expect is None:
            result['status'] = 'indeterminate' if overwritten else 'recorded'
        elif overwritten:
            result.update(status='indeterminate', reason='aux1 edges were overwritten in this window')
        elif len(mine) != len(expect):
            result.update(status='FAIL', reason='%s edge (%d of %d expected)' %
                          ('missing' if len(mine) < len(expect) else 'extra', len(mine), len(expect)))
        elif any(e['active'] != x['active'] for e, x in zip(mine, expect)):
            result.update(status='FAIL', reason='edge direction differs: aux1 %s, expected %s' % (
                [e['active'] for e in mine], [x['active'] for x in expect]))
        else:
            result['status'] = 'PASS'
        for edge, x in zip(mine, expect or []):
            item = {'edge_us': edge['at_us'], 'active': edge['active'], 'role': x.get('role')}
            if x.get('write'):
                register, value = x['write']['register'], x['write'].get('value')
                write = find_write(writes, register, value, row['start_us'], edge['at_us'] + 1)
                if write is None:
                    # The write may start after the edge (a reversed relationship).
                    write = find_write(writes, register, value, row['start_us'], row['end_us'])
                if write is None:
                    item['write_status'] = 'indeterminate: the write is missing from the debug traffic'
                else:
                    item['write'] = write
                    item['after_tx_start_us'] = edge['at_us'] - write['tx_start_us']
                    if write['ack_end_us'] is not None:
                        unc = write['uncertainty_us'] or 0
                        item['after_ack_end_us'] = [edge['at_us'] - write['ack_end_us'] - unc,
                                                    edge['at_us'] - write['ack_end_us'] + unc]
            if x.get('closure'):
                item['after_closure_us'] = [edge['at_us'] - x['closure']['latest_us'], edge['at_us'] - x['closure']['earliest_us']]
            if x.get('key'):
                item['drive'] = bracket(edge['at_us'], transitions(samples, tuple(x['key'])), x['new'])
            result['intervals'].append(item)
        out_rows.append(result)
    unattributed = [e for e in edges if e['at_us'] not in attributed]
    intervals = [i for r in out_rows for i in r['intervals']]
    measured = [i for i in intervals if i.get('drive', {}).get('status') == 'measured']
    relations = {}
    for i in measured:
        relations[i['drive']['relation']] = relations.get(i['drive']['relation'], 0) + 1
    summary = {'after_tx_start_us': stats([i.get('after_tx_start_us') for i in intervals]),
               'after_ack_end_us': stats([i['after_ack_end_us'][0] for i in intervals if 'after_ack_end_us' in i]),
               'after_closure_latest_us': stats([i['after_closure_us'][0] for i in intervals if 'after_closure_us' in i]),
               'drive_bracket_start_us': stats([i['drive']['relative_us'][0] for i in measured]),
               'drive_bracket_end_us': stats([i['drive']['relative_us'][1] for i in measured]),
               'drive_relations': relations,
               'drive_unmeasured': sum(1 for i in intervals if 'drive' in i and i['drive']['status'] != 'measured'),
               'write_missing': sum(1 for i in intervals if 'write_status' in i)}
    by_role = {}
    for role in sorted({i['role'] for i in intervals if i.get('role')}):
        mine = [i for i in intervals if i.get('role') == role]
        good = [i for i in mine if i.get('drive', {}).get('status') == 'measured']
        by_role[role] = {'n': len(mine),
                         'after_tx_start_us': stats([i.get('after_tx_start_us') for i in mine]),
                         'after_ack_end_us': stats([i['after_ack_end_us'][0] for i in mine if 'after_ack_end_us' in i]),
                         'after_closure_latest_us': stats([i['after_closure_us'][0] for i in mine if 'after_closure_us' in i]),
                         'drive_bracket_start_us': stats([i['drive']['relative_us'][0] for i in good]),
                         'drive_bracket_end_us': stats([i['drive']['relative_us'][1] for i in good])}
    summary['by_role'] = by_role
    return {'rows': out_rows, 'unattributed': unattributed, 'summary': summary,
            'failed': [r['label'] for r in out_rows if r['status'] == 'FAIL'],
            'indeterminate': [r['label'] for r in out_rows if r['status'] == 'indeterminate']}


def pair_events(edges: list, found: list, on_value: int, limit_us: int = 1000000) -> dict:
    """Pair aux1 edges with the drive transitions of one quantity (O6-O9 set
    and clear events). A transition to ``on_value`` should switch Y0 on (aux1
    inactive). Each edge takes the nearest unpaired transition of its
    direction within ``limit_us``; leftovers are reported, not hidden."""
    pairs, used = [], set()
    for edge in sorted(edges, key=lambda e: e['at_us']):
        value = on_value if not edge['active'] else 1 - on_value  # aux1 inactive: Y0 on.
        options = [(i, t) for i, t in enumerate(found) if i not in used and t['to'] == value]

        def distance(item):
            t = item[1]
            return 0 if t['earliest_us'] <= edge['at_us'] <= t['latest_us'] else \
                min(abs(edge['at_us'] - t['earliest_us']), abs(edge['at_us'] - t['latest_us']))
        best = min(options, key=distance, default=None)
        if best is None or distance(best) > limit_us:
            pairs.append({'edge_us': edge['at_us'], 'active': edge['active'], 'transition': None})
            continue
        used.add(best[0])
        t = best[1]
        relative = [t['earliest_us'] - edge['at_us'], t['latest_us'] - edge['at_us']]
        pairs.append({'edge_us': edge['at_us'], 'active': edge['active'], 'event': 'set' if value == on_value else 'clear',
                      'transition': t, 'relative_us': relative,
                      'relation': 'contains the edge' if relative[0] <= 0 <= relative[1] else
                      'before the edge' if relative[1] < 0 else 'after the edge'})
    return {'pairs': pairs, 'unpaired_edges': [p for p in pairs if p['transition'] is None],
            'unpaired_transitions': [t for i, t in enumerate(found) if i not in used],
            'set_bracket_end_us': stats([p['relative_us'][1] for p in pairs if p.get('event') == 'set']),
            'set_bracket_start_us': stats([p['relative_us'][0] for p in pairs if p.get('event') == 'set']),
            'clear_bracket_end_us': stats([p['relative_us'][1] for p in pairs if p.get('event') == 'clear']),
            'clear_bracket_start_us': stats([p['relative_us'][0] for p in pairs if p.get('event') == 'clear'])}


def split_rows(rows: list, end_us: int) -> list:
    """Close each row at the next row's start (the last at ``end_us``)."""
    ordered = sorted(rows, key=lambda r: r['start_us'])
    for row, nxt in zip(ordered, ordered[1:] + [None]):
        row['end_us'] = nxt['start_us'] if nxt else end_us
    return ordered


def mapping_class(custom: int, polarity: int, status_bit: int) -> str:
    """Output status bit (0x0009) against the custom request under inversion:
    `before` reports the request, `after` the inverted output."""
    if not polarity:
        return 'not_separated'
    if status_bit == custom:
        return 'before'
    if status_bit == custom ^ polarity:
        return 'after'
    return 'contradiction'


def experiment_class(observations: list) -> str:
    """O10/O11 class (Implement 3) from rows {custom, y0_on}: Y0 follows bit 0,
    bit 1 or experimental bit 2, no response to the tested bits, or bit 2 not
    tested (a planned bit-2 step without a measurement). ``y0_on`` is None for
    a step that was not measured. Anything else is indeterminate."""
    measured = [o for o in observations if o.get('y0_on') is not None]
    if not measured:
        return 'not measured'
    for bit, name in ((0, 'follows bit 0'), (1, 'follows bit 1'), (2, 'follows experimental bit 2')):
        if any((o['custom'] >> bit) & 1 for o in measured) and                 all(bool((o['custom'] >> bit) & 1) == bool(o['y0_on']) for o in measured):
            return name
    if any(o['y0_on'] for o in measured):
        return 'indeterminate'
    if any(o['custom'] & 4 for o in observations) and not any(o['custom'] & 4 for o in measured):
        return 'bit 2 not tested'
    return 'no response to the tested bits'


# --------------------------------------------------------------------------
# Live sequencing (one case per run; the scenario glue is bench_scenarios.py)

class Live:
    """Run state of one outputs case: traffic, samples, bench views, rows."""

    def __init__(self, console, run, baseline, args, facts, timeout):
        self.console, self.run, self.baseline, self.args, self.facts = console, run, baseline, args, facts
        self.timeout = timeout
        self.case = CASES[args.case]
        self.traffic, self.samples, self.views, self.rows, self.events = [], [], [], [], []
        self.snapshots = {}
        self.observations = []

    def tap(self, emit):
        def handler(event, **fields):
            response = fields.get('response')
            if event == 'traffic' and len(self.traffic) < 20000:
                self.traffic.append(response)
            elif event == 'reply' and isinstance(response, dict) and response.get('command') == 'read-state' and \
                    response.get('type') == 'read' and response.get('address') == OWNER:
                sample = state_sample(response)
                if sample:
                    self.samples.append(sample)
            emit(event, **fields)
        return handler

    def call(self, name, **kwargs):
        reply = self.console.command(name, timeout_s=self.timeout, **kwargs)
        self.events.append({'command': name, 'arguments': {k: v for k, v in kwargs.items()},
                            'ok': reply.get('ok'), 'result': reply.get('result'), 'outcome': reply.get('outcome'),
                            'operation_id': reply.get('operation_id'), 'started_us': reply.get('started_us'),
                            'serviced_us': reply.get('serviced_us')})
        return reply

    def checked(self, name, **kwargs):
        reply = self.call(name, **kwargs)
        if not reply.get('ok'):
            raise probe.BenchError('%s refused/failed: %s' % (name, reply.get('result', reply.get('outcome'))))
        return reply

    def state(self):
        return self.call('read-state', address=OWNER)

    def view(self):
        view = self.checked('bench', host_args=())
        self.views.append(view)
        return view

    def now_us(self):
        """Controller time now, from a bench view (a local command, no bus traffic)."""
        return self.view()['now_us']


def io_write(live, key, value, setup=True):
    """One ordinary setting write of drive 4 through its planned route (the
    stated Y0 facts for I/O, CONFIGURE records otherwise); read back."""
    entry = {'drive': OWNER, 'key': key, 'index': None, 'candidate': value}
    steps, refusal = records.plan_entry(live.baseline, entry, live.args.image_level, live.facts, restore=False)
    if refusal:
        raise probe.BenchError('no ordinary route for %s: %s' % (key, refusal['reason']))
    step = dict(steps[0], target=value, candidate=value)
    command, kwargs, text = records.write_call({'key': key, 'index': None}, value, OWNER)
    step['write'] = {'command': command, 'kwargs': kwargs, 'text': text}
    outcome = records.apply_write(live.console, step, timeout_s=live.timeout)
    live.run.field(entry, baseline=step['baseline'])
    reply = outcome['reply']
    live.events.append({'command': text, 'ok': reply.get('ok'), 'operation_id': reply.get('operation_id'),
                        'started_us': reply.get('started_us'), 'outcome': reply.get('outcome'),
                        'readback': (reply.get('observation') or {}).get('raw')})
    if outcome['uncertain'] or not outcome['written']:
        live.run.field(entry, uncertain=outcome['uncertain'],
                       reason='outputs write %s %s refused or uncertain; not replayed' % (key, value))
        raise probe.BenchError('write %s %s refused or uncertain: %s' % (key, value, outcome['result']))
    if setup:
        # Setup writes get their own recorded row: their edges are not a stimulus row's.
        live.rows.append({'label': 'setup %s %s' % (key, value), 'start_us': reply['started_us'], 'expect': None})
    return reply


def _fresh_motion_reads(live):
    live.checked('read-config', address=OWNER)
    return live.checked('read-state', address=OWNER)


def _poll_until(live, handle, done, limit=400):
    """read state, one at a time, until ``done(sample)`` or the handle ends; bench view every 4th."""
    for index in range(limit):
        reply = live.state()
        sample = state_sample(reply)
        if index % 4 == 3:
            live.view()
        if done(sample, handle):
            return sample
    raise probe.BenchError('poll bound exhausted')


def move(live, steps, stop_after_s=None):
    """One finite relative move at 60 rpm with read-state polls throughout.
    Optional ``stop_after_s``: `stop fast` once RUNNING was reported and that
    long has passed. Returns (terminal, stop reply or None)."""
    _fresh_motion_reads(live)
    handle = live.console.begin('move-relative', move_args=(str(steps), 'steps', 'native', '60', 'configured'),
                                address=OWNER, timeout_s=max(live.timeout, 12))
    if not handle.accepted:
        terminal = live.console.wait(handle)
        raise probe.BenchError('move refused before transmission: %s' % terminal.get('result'))
    began = time.monotonic()
    stop = None
    running_seen = False
    for _ in range(600):
        sample = state_sample(live.state())
        running_seen = running_seen or bool(sample and sample['motion'] & RUNNING)
        if stop_after_s is not None and stop is None and running_seen and time.monotonic() - began >= stop_after_s:
            stop = live.call('stop', stop_policy='fast', address=OWNER)
        if handle.terminal is not None and sample and not sample['motion'] & RUNNING and sample['speed'] == 0:
            break
    terminal = live.console.wait(handle, release=True)
    for _ in range(3):
        live.state()
    live.view()
    live.events.append({'command': 'move-relative %d' % steps, 'ok': terminal.get('ok'),
                        'operation_id': terminal.get('operation_id'), 'started_us': terminal.get('started_us'),
                        'outcome': terminal.get('outcome'), 'interrupted_by_stop': terminal.get('interrupted_by_stop')})
    return terminal, stop


def _io_reads(live):
    live.checked('read-config', address=OWNER)
    live.checked('io', driver_args=('read',), address=OWNER)
    return live.checked('read-state', address=OWNER)


def _aux1(view):
    return view['input']['active']


def run_custom(live):
    """O1/O2: Y0 = 9 (and polarity 1 for O2), custom bit 0 on/off N cycles."""
    polarity = live.case['polarity']
    io_write(live, 'io.y0', 9)
    if polarity:
        io_write(live, 'io.output-polarity', 1)
    live.view()
    for cycle in range(live.args.repetitions):
        for bit in (1, 0):
            reply = io_write(live, 'io.custom', bit, setup=False)
            after = state_sample(live.state())
            view = live.view()
            y0_on = bool(bit) != bool(polarity)  # The inversion hypothesis; O2 records the measured relation.
            live.rows.append({'label': 'cycle %d custom %d' % (cycle + 1, bit), 'start_us': reply['started_us'],
                              'expect': [{'active': not y0_on, 'write': {'register': 0x4F, 'value': bit},
                                          'closure': closure(reply, 0x4F), 'role': 'custom %d' % bit,
                                          'key': ('outputs', 1), 'new': (after['outputs'] & 1) if after else bit}]})
            live.observations.append({'custom': bit, 'polarity': polarity, 'aux1_active': _aux1(view),
                                      'status_bit': after['outputs'] & 1 if after else None})


def run_levels(live):
    """O3: Y0 = 0 (function 0); aux1 and 0x0009 at polarity 0, then 1."""
    after = state_sample(live.state())
    view = live.view()
    live.observations.append({'polarity': 0, 'aux1_active': _aux1(view), 'status_bit': after['outputs'] & 1 if after else None})
    io_write(live, 'io.output-polarity', 1)
    after = state_sample(live.state())
    view = live.view()
    live.observations.append({'polarity': 1, 'aux1_active': _aux1(view), 'status_bit': after['outputs'] & 1 if after else None})


def _move_row(live, label, terminal, stop, y0_running_on):
    """Running (O4/O5): ON at the start write, OFF at completion or at the stop write."""
    off = {'active': y0_running_on, 'key': ('motion', RUNNING), 'new': 0, 'role': 'stop' if stop else 'completion',
           'write': {'register': MOTION_REGISTER, 'value': STOP_FAST} if stop else None,
           'closure': closure(stop) if stop else None}
    live.rows.append({'label': label, 'start_us': terminal['started_us'],
                      'expect': [{'active': not y0_running_on, 'key': ('motion', RUNNING), 'new': 1, 'role': 'start',
                                  'write': {'register': MOTION_REGISTER, 'value': START_RELATIVE},
                                  'closure': closure(terminal)}, off]})


def run_running(live):
    """O4: Y0 = 2; 1000- and 3000-step moves and one stop fast mid-move. O5: polarity 1, one move."""
    io_write(live, 'io.y0', 2)
    polarity = live.case['polarity']
    if polarity:
        io_write(live, 'io.output-polarity', 1)
    _fresh_motion_reads(live)
    live.snapshots['profile'] = _profile(live, 'read')
    live.view()
    plan = [1000] if live.args.case == 'O5' else [MOVE_STEPS[i % 4] for i in range(live.args.repetitions)]
    for index, steps in enumerate(plan):
        terminal, _ = move(live, steps)
        _move_row(live, 'move %d (%+d)' % (index + 1, steps), terminal, None, not polarity)
    if live.args.case == 'O4':
        terminal, stop = move(live, 3000, stop_after_s=.8)
        if stop is None:
            raise probe.BenchError('no RUNNING report before the move ended; stop row NOT RUN')
        _move_row(live, 'move +3000 stop fast', terminal, stop, True)
        terminal, _ = move(live, -3000)
        _move_row(live, 'return -3000', terminal, None, True)


def _profile(live, action):
    import bench_motion as motion
    reply = motion.profile_snapshot(live.console, action, timeout_s=live.timeout)
    live.events.append({'command': 'motion-profile %s' % action, 'ok': reply.get('ok'), 'original': reply.get('original'),
                        'current': reply.get('current')})
    return {k: reply.get(k) for k in ('original', 'current', 'restored', 'session_ok')}


def profile_refresh(live):
    """Refresh the profile session before a move that follows a home or a
    recovery: `motion-profile read` keeps the saved snapshot; when the binding
    changed (recovery) it is refused, and the snapshot is forgotten and taken
    again. The baseline words are then restored by the planner's stage route,
    not by this replacement snapshot."""
    try:
        _profile(live, 'read')
    except probe.BenchError:
        live.checked('motion-profile', host_args=('forget',))
        live.snapshots['profile_replaced'] = True
        live.snapshots['profile_replacement'] = _profile(live, 'read')


def relation_refresh(live):
    """A profile stage earlier in this controller session revokes the drive's
    start-speed relation, and a home's generation change expires an installed
    one. Restate the startup policy (stored start word W, minimum W rpm) after
    a fresh profile read; the moves run at 60 rpm. A stated fact, recorded."""
    word = str((live.snapshots.get('profile_replacement') or live.snapshots['profile'])['current'][0])
    reply = live.call('motion-profile-start-speed-relation', host_args=(word, word))
    live.events.append({'command': 'motion-profile start-speed relation %s %s' % (word, word), 'reply': reply.get('result'),
                        'ok': reply.get('ok')})


def run_arrival(live):
    """O6: Y0 = 4; complete moves, a stop fast short of target, position clear."""
    io_write(live, 'io.y0', 4)
    _fresh_motion_reads(live)
    live.snapshots['profile'] = _profile(live, 'read')
    live.view()
    for index in range(live.args.repetitions):
        steps = 1000 if index % 2 == 0 else -1000
        terminal, _ = move(live, steps)
        live.rows.append({'label': 'move %d (%+d)' % (index + 1, steps), 'start_us': terminal['started_us'], 'expect': None})
    terminal, stop = move(live, 3000, stop_after_s=.8)
    live.rows.append({'label': 'move +3000 stop fast', 'start_us': terminal['started_us'], 'expect': None})
    terminal, _ = move(live, -1000)
    live.rows.append({'label': 'move -1000 after the stop', 'start_us': terminal['started_us'], 'expect': None})
    _fresh_motion_reads(live)
    live.checked('position-clear-qualification-install', host_args=('clear-semantics',))
    clear = live.call('position-clear', address=OWNER)
    for _ in range(4):
        live.state()
    live.view()
    live.rows.append({'label': 'position-clear', 'start_us': clear.get('started_us') or live.now_us(), 'expect': None})


def _home(live, method):
    live.checked('read-config', address=OWNER)
    live.checked('read-state', address=OWNER)
    live.checked('home-read', address=OWNER)
    facts = ('mechanics', 'rates', 'ramp', 'zero-offset', 'auxiliary', 'inputs')
    if method != 35:
        facts += ('index', 'reference')
    live.checked('home-qualification-install', host_args=(str(method), '60', '30', '100', *facts))
    handle = live.console.begin('home', home_args=(str(method), '60', '30', '100', 'zero'), address=OWNER,
                                timeout_s=max(live.timeout, 35))
    if not handle.accepted:
        terminal = live.console.wait(handle)
        raise probe.BenchError('home %d refused: %s' % (method, terminal.get('result')))
    for _ in range(600):
        sample = state_sample(live.state())
        if handle.terminal is not None and sample and not sample['motion'] & RUNNING and sample['speed'] == 0:
            break
    terminal = live.console.wait(handle, release=True)
    for _ in range(3):
        live.state()
    live.view()
    live.events.append({'command': 'home %d' % method, 'ok': terminal.get('ok'), 'operation_id': terminal.get('operation_id'),
                        'started_us': terminal.get('started_us'), 'outcome': terminal.get('outcome')})
    if not terminal.get('ok'):
        raise probe.BenchError('home %d did not complete: %s' % (method, terminal.get('outcome')))
    return terminal


def run_homing(live):
    """O7: Y0 = 3; home 35 (from a not-homed state), then repeated (a move, home 33).
    A second home 35 cannot show a fresh HOMED transition (no motion, HOMED
    already set), so the repetitions use method 33, which searches the index."""
    io_write(live, 'homing.auxiliary', 7)  # The planned route: reads, CONFIGURE facts, readback.
    io_write(live, 'io.y0', 3)
    _fresh_motion_reads(live)
    live.snapshots['profile'] = _profile(live, 'read')
    relation_refresh(live)
    live.view()
    first = state_sample(live.state())
    if first and not first['motion'] & HOMED:
        terminal = _home(live, 35)
        live.rows.append({'label': 'home 35', 'start_us': terminal['started_us'], 'expect': None})
    else:
        live.events.append({'command': 'home 35 skipped', 'reason': 'HOMED already set: no fresh completion to observe'})
    for index in range(live.args.repetitions):
        steps = 200 if index % 2 == 0 else -200
        _fresh_motion_reads(live)
        profile_refresh(live)
        relation_refresh(live)
        terminal, _ = move(live, steps)
        live.rows.append({'label': 'move %+d (%d)' % (steps, index + 1), 'start_us': terminal['started_us'], 'expect': None})
        terminal = _home(live, 33)
        live.rows.append({'label': 'home 33 (%d)' % (index + 1), 'start_us': terminal['started_us'], 'expect': None})


def run_brake(live):
    """O8: Y0 = 5; motor-release, enable (1 s wait), N cycles."""
    io_write(live, 'io.y0', 5)
    live.state()
    live.view()
    # Assigned while enabled, function 5 leaves Y0 off until the next enable
    # (wired bench 06 O8 diagnosis): one recorded cycle sets the brake output.
    for label, command in (('setup release', 'motor-release'), ('setup enable', 'enable')):
        reply = live.checked(command, address=OWNER)
        for _ in range(3):
            live.state()
        live.view()
        live.rows.append({'label': label, 'start_us': reply['started_us'], 'expect': None})
    time.sleep(1.0)
    for index in range(live.args.repetitions):
        release = live.checked('motor-release', address=OWNER)
        for _ in range(3):
            live.state()
        live.view()
        live.rows.append({'label': 'release %d' % (index + 1), 'start_us': release['started_us'],
                          'expect': [{'active': True, 'write': {'register': AUXILIARY_REGISTER, 'value': RELEASE},
                                      'closure': closure(release), 'key': ('motion', RELEASED), 'new': 1,
                                      'role': 'release'}]})
        enable = live.checked('enable', address=OWNER)
        for _ in range(3):
            live.state()
        live.view()
        live.rows.append({'label': 'enable %d' % (index + 1), 'start_us': enable['started_us'],
                          'expect': [{'active': False, 'write': {'register': AUXILIARY_REGISTER, 'value': ENABLE},
                                      'closure': closure(enable), 'key': ('motion', RELEASED), 'new': 0,
                                      'role': 'enable'}]})
        time.sleep(1.0)


def run_alarm(live):
    """O9: Y0 = 1, deviation threshold 1; a 2000-step move latches alarm 5; recovery, alarm-clear."""
    io_write(live, 'filters.deviation-threshold', 1)  # The planned route: reads, CONFIGURE facts, readback.
    io_write(live, 'io.y0', 1)
    _fresh_motion_reads(live)
    live.snapshots['profile'] = _profile(live, 'read')
    live.view()
    for index in range(live.args.repetitions):
        _fresh_motion_reads(live)
        if index:
            profile_refresh(live)  # The recovery after the previous alarm changed the binding.
            _fresh_motion_reads(live)
        handle = live.console.begin('move-relative', move_args=('2000', 'steps', 'native', '60', 'configured'),
                                    address=OWNER, timeout_s=max(live.timeout, 12))
        terminal = live.console.wait(handle, release=True)
        live.events.append({'command': 'move-relative 2000 (alarm)', 'ok': terminal.get('ok'),
                            'operation_id': terminal.get('operation_id'), 'started_us': terminal.get('started_us'),
                            'outcome': terminal.get('outcome')})
        time.sleep(2.0)  # No reply right after the start is expected; never resend the move.
        recovered = None
        for _ in range(3):
            reply = live.state()
            if reply.get('ok'):
                break
            if reply.get('result') == 'recovery_required':
                recovered = live.call('recover')
        live.view()
        live.rows.append({'label': 'alarm %d' % (index + 1), 'start_us': terminal.get('started_us') or live.now_us(),
                          'expect': [{'active': False, 'key': ('alarm',), 'new': 1, 'role': 'alarm'}]})
        live.events.append({'command': 'recovery', 'recovered': recovered is not None and recovered.get('ok')})
        live.call('stop', stop_policy='fast', address=OWNER)
        live.state()
        clear = live.checked('alarm-clear', address=OWNER)
        for _ in range(4):
            live.state()
        live.view()
        live.rows.append({'label': 'alarm-clear %d' % (index + 1), 'start_us': clear['started_us'],
                          'expect': [{'active': True, 'write': {'register': AUXILIARY_REGISTER, 'value': CLEAR_ALARM},
                                      'closure': closure(clear), 'key': ('alarm',), 'new': 0, 'role': 'alarm-clear'}]})


RUNNERS = {'custom': run_custom, 'levels': run_levels, 'running': run_running, 'arrival': run_arrival,
           'homing': run_homing, 'brake': run_brake, 'alarm': run_alarm}

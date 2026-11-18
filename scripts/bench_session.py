"""Shared finite bench session, incremental evidence and bounded summaries.

Uses the production Console parser; owns one port until scenario cleanup ends.
Framing/evidence failure forbids further I/O. No recovery or replay is implicit.
"""
import hashlib
import json
import os
from pathlib import Path
import time

from bench_probe import BenchError, Console, Evidence, debug_session, open_port

CACHED = ('version', 'host', 'config', 'load', 'drv', 'memory', 'stats', 'status', 'health', 'wiring')


def checked(console, command, *, timeout_s=5, **arguments):
    result = console.command(command, timeout_s=timeout_s, **arguments)
    if not result['ok']:
        raise BenchError(command + ' refused/failed: ' + str(result.get('result', result.get('outcome'))))
    return result


def snapshot(console, *, address=1, refresh=False, timeout_s=5):
    """Fixed-size local diagnostics; optional explicit three-kind motor reads."""
    out = {name: checked(console, name, timeout_s=timeout_s) for name in CACHED}
    out['console_config'] = out.pop('config')
    if refresh:
        for kind in ('identity', 'config', 'state'):
            out[kind] = checked(console, 'read-' + kind, address=address, timeout_s=timeout_s)
    return out


def cached_failure(console, record, *, timeout_s=5):
    """No new motor work after failure; stop on the first diagnostic failure."""
    record['failure_diagnostics'] = {}
    if not console.synchronized:
        record['failure_diagnostics_error'] = 'framing unavailable; no diagnostic command sent'
        return
    for name in CACHED:
        try:
            record['failure_diagnostics'][name] = checked(console, name, timeout_s=timeout_s)
        except BaseException as error:
            record['failure_diagnostics_error'] = str(error) or 'interrupted'
            break


def verify_restoration(record):
    """Scan completion must agree with the subsequently observed host/selection."""
    scan = record.get('scan')
    if scan is None:
        return
    host = record['after']['host']
    wiring = record['after']['wiring']
    target = [wiring['target'], wiring['address'], wiring['generation']]
    if (not scan['restored'] or not host['active_known'] or host['blocked'] or
            host['configuring'] or host['active'] != scan['original_tuple'] or
            target != scan['original_target']):
        raise BenchError('scan restoration disagrees with actual host/selection; explicit repair required')


def _firmware(path):
    if path is None:
        return None  # Console version is not an exact flash-image hash.
    path = Path(path)
    digest = hashlib.sha256()
    with path.open('rb') as image:
        for chunk in iter(lambda: image.read(65536), b''):
            digest.update(chunk)
    return dict(path=str(path), bytes=path.stat().st_size, sha256=digest.hexdigest(),
                provenance='caller supplied image; console does not attest flash hash')


class _Transcript:
    """Port proxy that copies every written console line to a text transcript."""

    def __init__(self, port, stream):
        self.port, self.stream = port, stream

    def read(self, size):
        return self.port.read(size)

    def write(self, data):
        written = self.port.write(data)
        self.stream.write('> ' + bytes(data).decode('utf-8', 'backslashreplace').rstrip('\r\n') + '\n')
        self.stream.flush()
        return written


def run_recorded(*, port, out=None, scenario, inputs, run, address=1, timeout_s=5,
                 debug=None, firmware=None, refresh=True, max_records=20000,
                 max_bytes=16 * 1024 * 1024, port_opener=open_port, paths=None):
    """Reserve exclusive artifacts before opening one port; write summary on error.

    ``run(console, record)`` performs a finite named scenario and its preplanned
    cleanup. Summaries retain at most 16 campaign records, never raw line history.
    Before/after snapshots have fixed command counts. Exact lines stream to JSONL.
    Returns the summary; any scenario/cleanup/framing failure raises after it is
    written. A failed debug-mode restoration also remains a failure.

    ``paths`` (run-folder layout) names the exclusive ``summary``, ``jsonl``
    and ``console`` files instead of the ``out`` prefix; the console file is a
    plain transcript (``>`` sent, ``<`` received lines).
    """
    if paths is None:
        prefix = Path(out)
        prefix.parent.mkdir(parents=True, exist_ok=True)
        paths = dict(summary=prefix.with_suffix('.json'), jsonl=prefix.with_suffix('.jsonl'), console=None)
    record = dict(scenario=scenario, inputs=inputs, port=port, ok=False,
                  automatic_recovery=False, automatic_write_replay=False,
                  independent_shaft_observation='unmeasured', electrical_timing='unmeasured',
                  firmware_artifact=_firmware(firmware), campaigns=[], campaigns_seen=0,
                  campaigns_failed=0)
    # Every exclusive reservation precedes port ownership and device commands.
    with Path(paths['summary']).open('x', encoding='utf-8', newline='\n') as summary, \
            Path(paths['jsonl']).open('x', encoding='utf-8', newline='\n') as log, \
            (Path(paths['console']).open('x', encoding='utf-8', newline='\n')
             if paths.get('console') else open(os.devnull, 'w', encoding='utf-8')) as transcript:
        evidence = Evidence(log, max_records=max_records, max_bytes=max_bytes)
        keep_transcript = bool(paths.get('console'))

        def emit(event, **fields):
            if keep_transcript and event == 'line':
                transcript.write('< ' + fields.get('text', '') + '\n')
                transcript.flush()
            evidence(event, **fields)
            if event in ('summary', 'discovery_summary'):
                record['campaigns_seen'] += 1
                if fields.get('ok') is False:
                    record['campaigns_failed'] += 1
                record['campaigns'].append(dict(event=event, **fields))
                del record['campaigns'][:-16]

        started = time.monotonic()
        primary_error = None
        try:
            with port_opener(port, 115200, timeout_s) as connection:
                console = None
                if keep_transcript:
                    connection = _Transcript(connection, transcript)
                try:
                    console = Console(connection, on_event=emit)
                    console.drain_startup(.5)
                    record['version'] = console.identify(timeout_s=timeout_s)
                    record['before'] = snapshot(console, address=address, refresh=refresh, timeout_s=timeout_s)
                    with debug_session(console, debug, timeout_s) as diagnostics:
                        record['debug'] = diagnostics
                        run(console, record)
                    if record['campaigns_failed'] or record.get('workload_verified') is not True:
                        raise BenchError('requested workload did not complete; cleanup does not convert it to PASS')
                    # Full after-values are new explicit reads on success only.
                    record['after'] = snapshot(console, address=address, refresh=refresh, timeout_s=timeout_s)
                    verify_restoration(record)
                    record['ok'] = True
                except BaseException as error:
                    primary_error = error
                    record['error'] = str(error) or 'interrupted'
                    if console is not None:
                        cached_failure(console, record, timeout_s=timeout_s)
                    raise
        except BaseException as error:
            record['ok'] = False  # Closing the port is part of this owned session.
            record.setdefault('error', str(error) or 'interrupted')
            if primary_error is not None and error is not primary_error:
                record['owner_error'] = str(error) or 'interrupted'
                raise primary_error from error
            raise
        finally:
            record['elapsed_s'] = time.monotonic() - started
            record['evidence'] = dict(records=evidence.records, bytes=evidence.bytes,
                                      exhausted=evidence.exhausted, max_records=max_records,
                                      max_bytes=max_bytes)
            summary.write(json.dumps(record, indent=2, allow_nan=False) + '\n')
            summary.flush()
    return record

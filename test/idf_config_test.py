"""Compile the actual USB adapter with valid and console-corrupting SDK settings,
and check that both builds declare the same board fixture wiring table."""
import re
import subprocess
import sys
import tempfile
from pathlib import Path


def fixture_tables(root):
    """Kconfig defaults, BoardPins.h and the IDF fixture test defines, as entry lists."""
    kconfig = (root / 'examples/probe_idf/main/Kconfig.projbuild').read_text(encoding='utf-8')
    defaults = dict(re.findall(r'config (MOTORCONTROLRS_FIXTURE_SLOT\d_\w+)\n(?:\s+.*\n)*?\s+default (\S+)', kconfig))
    assert re.search(r'config MOTORCONTROLRS_FIXTURE_WIRING\n\s+bool .*\n\s+default y', kconfig), 'fixture table must default on'
    defines = dict(re.findall(r'#define CONFIG_(MOTORCONTROLRS_FIXTURE_SLOT\d_\w+) (\S+)',
                              (root / 'test/fixture_wiring_app_test.cpp').read_text(encoding='utf-8')))
    def slots(values):
        table = []
        for slot in range(1, 5):
            entry = tuple(int(values[f'MOTORCONTROLRS_FIXTURE_SLOT{slot}_{name}'], 0) for name in ('ADDRESS', 'INPUTS', 'OUTPUTS'))
            if entry[0]:
                table.append(entry)
        return table
    board = (root / 'examples/common/BoardPins.h').read_text(encoding='utf-8')
    body = re.search(r'kFixtureWiring\[\] = \{(.*?)\};', board).group(1)
    pins = [tuple(int(v, 0) for v in item.split(',')) for item in re.findall(r'\{([^{}]*)\}', body)]
    return slots(defaults), pins, slots(defines)


def main():
    compiler, compiler_id = sys.argv[1:]
    root = Path(__file__).resolve().parents[1]
    kconfig, pins, defines = fixture_tables(root)
    assert kconfig == pins == defines == [(1, 0xF, 0x0), (2, 0xF, 0x0), (4, 0xF, 0x1)], (kconfig, pins, defines)
    source = root / 'examples/probe_idf/main/IdfPlatform.cpp'
    includes = [root / 'test/fakes/esp32_uart', root / 'examples/probe_cli']
    with tempfile.TemporaryDirectory() as temporary:
        if compiler_id == 'MSVC':
            command = [compiler, '/nologo', '/c', '/std:c++14',
                       '/DCONFIG_IDF_TARGET_ESP32S3=1',
                       *['/I' + str(path) for path in includes],
                       '/Fo' + str(Path(temporary) / 'platform.obj'), str(source)]
            debug_option = '/DCONFIG_GPTIMER_ENABLE_DEBUG_LOG=1'
        else:
            command = [compiler, '-std=c++11', '-fsyntax-only',
                       '-DCONFIG_IDF_TARGET_ESP32S3=1',
                       *['-I' + str(path) for path in includes], str(source)]
            debug_option = '-DCONFIG_GPTIMER_ENABLE_DEBUG_LOG=1'
        normal = subprocess.run(command, capture_output=True, text=True)
        assert normal.returncode == 0, normal.stdout + normal.stderr
        debug = subprocess.run(command + [debug_option], capture_output=True, text=True)
        assert debug.returncode != 0, 'Forced GPTimer logging must prevent firmware compilation'
        assert 'SDK logging must remain disabled' in debug.stdout + debug.stderr
        arduino = command.copy()
        arduino[arduino.index(str(source))] = str(root / 'examples/probe_cli/ArduinoPlatform.cpp')
        prefix = '/D' if compiler_id == 'MSVC' else '-D'
        normal = subprocess.run(arduino + [prefix + 'ARDUINO_USB_CDC_ON_BOOT=0'], capture_output=True, text=True)
        assert normal.returncode == 0, normal.stdout + normal.stderr
        competing = subprocess.run(arduino + [prefix + 'ARDUINO_USB_CDC_ON_BOOT=1'], capture_output=True, text=True)
        assert competing.returncode != 0, 'Arduino HWCDC must not compete with the shared IDF USB driver'
        assert 'Arduino CDC auto-start must be disabled' in competing.stdout + competing.stderr
    print('Native SDK configuration and exclusive Arduino USB-driver ownership verified')


if __name__ == '__main__':
    main()

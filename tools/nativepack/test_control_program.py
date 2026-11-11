"""Exercise the actual shared C++ parser through the authoring adapter."""
import configparser
from pathlib import Path
import tempfile
import unittest

from nativepack import author_format3, control_program

LIBRARY = Path(__file__).resolve().parents[2] / 'bin/author-assets/eff_author_assets_format3.dll'
SOURCE = '''; Author-owned declarations, independent of any particular effect.
[ParameterMode]
variable=$mode
type=bool
default=false
public=true
persist=true
label=模式

[ParameterValue]
variable=$value
default=0.25
min=0
max=1
public=true

[ParameterResult]
variable=$result
value=$value * 2 + 1
max=3
public=true

[ActionToggle]
label=切换
public=true
code=
    if $mode
        $mode = false
    else
        $mode = true
    endif

[EventToggle]
key=F7
action=ActionToggle

[UIRoot]
type=group
label=控制
expanded=true

[UIMode]
type=checkbox
parent=UIRoot
variable=$mode
'''


@unittest.skipUnless(LIBRARY.is_file(), 'build the shared Format 3 author tools first')
class ControlProgramTests(unittest.TestCase):
    def setUp(self):
        control_program.configure(LIBRARY)

    def read(self, root):
        author_format3.read_format3_directory(root)
        ini = configparser.ConfigParser(interpolation=None, delimiters=('=',))
        ini.optionxform = str
        ini.read(root / 'mod.ini', encoding='utf-8-sig')
        return author_format3.read_controls_ini(root, ini)

    def test_shared_parser_keeps_editable_source(self):
        result = control_program.describe(SOURCE)
        self.assertEqual(result['defaults']['$value'], .25)
        self.assertEqual(len(result['parameters']), 3)
        self.assertIn('if $mode', result['source']['ActionToggle'])
        self.assertIn('切换', result['source']['ActionToggle'])
        self.assertTrue(next(p for p in result['parameters'] if p['variable'] == '$result')['computed'])

    def test_invalid_program_reports_section_and_line(self):
        with self.assertRaisesRegex(ValueError, r'ActionToggle.*line'):
            control_program.describe(SOURCE.replace('$mode = false', '$missing = false'))
        with self.assertRaisesRegex(ValueError, 'read only'):
            control_program.describe(SOURCE.replace('$mode = false', '$result = 1'))
        with self.assertRaisesRegex(ValueError, 'cycle'):
            control_program.describe(SOURCE + '[ActionLoop]\ncode=call ActionLoop\n')
        with self.assertRaisesRegex(ValueError, 'order'):
            control_program.describe(SOURCE.replace('type=group', 'type=group\norder=1e40'))
        with self.assertRaisesRegex(ValueError, 'persist'):
            control_program.describe('[Constants]\npersist $derived=0\n' + SOURCE +
                '[ParameterDerived]\nvariable=$derived\nvalue=$value\n')

    def test_indented_next_section_is_not_an_action_instruction(self):
        result = control_program.describe(SOURCE.replace('[EventToggle]', '    [EventToggle]'))
        self.assertIn('EventToggle', result['source'])

    def test_export_import_export_preserves_program_and_resource_independence(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            author_format3.write_format3_directory(root, 'Unified', [], {}, [], [], [], [], [],
                control_program_source=SOURCE)
            state = self.read(root)
            self.assertEqual(state['defaults']['$value'], .25)
            self.assertEqual(state['program_source']['ActionToggle'].count('$mode = true'), 1)
            self.assertEqual(set(p.name for p in root.iterdir()), {'mod.ini'})
            second = root / 'second'
            second.mkdir()
            author_format3.write_format3_directory(second, 'UnifiedCopy', [], {}, [], [], [], [], [],
                state_defaults=state['defaults'], persistent_states=state['persistent'],
                preserved_controls=state['controls'], control_program_source=state['program_source'])
            again = self.read(second)
            self.assertEqual({k: v.strip() for k, v in again['program_source'].items()},
                             {k: v.strip() for k, v in state['program_source'].items()})
            self.assertEqual(again['defaults'], state['defaults'])
            edited = SOURCE.replace('default=0.25', 'default=0.75')
            author_format3.write_format3_directory(second, 'UnifiedCopy', [], {}, [], [], [], [], [],
                state_defaults=state['defaults'], persistent_states=state['persistent'],
                preserved_controls=state['controls'], control_program_source=edited)
            self.assertEqual(self.read(second)['defaults']['$value'], .75)

    def test_material_binding_uses_generic_value_and_declared_target(self):
        source = SOURCE + '''[MaterialExample]
path=unused.effres
[BindingExample]
target=material.float
material=MaterialExample
property=_Example
value=$result
'''
        result = control_program.describe(source)
        self.assertEqual(result['bindings'][0]['variable'], '$binding_BindingExample')
        self.assertEqual(result['bindings'][0]['value'], '$result')

    def test_only_widget_owned_parameter_fields_override_retained_declarations(self):
        control = dict(enabled=True, identity='Smile', variable='$value', shape='Smile',
                       label='Edited Smile', default=.75, minimum=0, maximum=2,
                       hotkey_increase='', hotkey_decrease='', hotkey_speed=1)
        state = author_format3._controls_resource('Widgets',
            [dict(partId='MeshA', object='A', shapeControls=[control])], [], [], [],
            state_defaults={'$value': .25, '$mode': 0, '$result': 1.5}, return_state=True)
        ini = configparser.ConfigParser(interpolation=None, delimiters=('=',))
        ini.optionxform = str
        ini.read_string(SOURCE)
        updated = author_format3._widget_control_program_source(SOURCE, ini, state)
        before, after = control_program.describe(SOURCE), control_program.describe(updated)
        self.assertEqual(after['defaults']['$value'], .75)
        self.assertEqual(next(p for p in after['parameters'] if p['variable'] == '$value')['max'], 2)
        for section in ('ParameterMode', 'ParameterResult', 'ActionToggle', 'EventToggle', 'UIRoot', 'UIMode'):
            self.assertEqual(before['source'][section], after['source'][section])

    def test_widget_default_is_added_when_parameter_had_implicit_default(self):
        source = '[ParameterValue]\nvariable=$value\npublic=true\n'
        ini = configparser.ConfigParser(interpolation=None, delimiters=('=',))
        ini.optionxform = str
        ini.read_string(source)
        state = dict(authored_defaults={'$value'}, defaults={'$value': .75}, authored_controls=set())
        updated = author_format3._widget_control_program_source(source, ini, state)
        self.assertEqual(control_program.describe(updated)['defaults']['$value'], .75)


if __name__ == '__main__':
    unittest.main()

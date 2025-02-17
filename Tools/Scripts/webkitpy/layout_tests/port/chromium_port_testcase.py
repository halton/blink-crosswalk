# Copyright (C) 2010 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#    * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#    * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import webkitpy.thirdparty.unittest2 as unittest

from webkitpy.common.system import logtesting
from webkitpy.common.system.executive_mock import MockExecutive2
from webkitpy.common.system.systemhost_mock import MockSystemHost
from webkitpy.tool.mocktool import MockOptions

import android
import linux
import mac
import win

from webkitpy.layout_tests.models.test_configuration import TestConfiguration
from webkitpy.layout_tests.port import port_testcase


class FakePrinter(object):
    def write_update(self, msg):
        pass

    def write_throttled_update(self, msg):
        pass


class ChromiumPortTestCase(port_testcase.PortTestCase):

    def test_check_build(self):
        port = self.make_port()
        port.check_build(needs_http=True, printer=FakePrinter())

    def test_default_max_locked_shards(self):
        port = self.make_port()
        port.default_child_processes = lambda: 16
        self.assertEqual(port.default_max_locked_shards(), 4)
        port.default_child_processes = lambda: 2
        self.assertEqual(port.default_max_locked_shards(), 1)

    def test_default_pixel_tests(self):
        self.assertEqual(self.make_port().default_pixel_tests(), True)

    def test_missing_symbol_to_skipped_tests(self):
        # Test that we get the chromium skips and not the webkit default skips
        port = self.make_port()
        skip_dict = port._missing_symbol_to_skipped_tests()
        if port.PORT_HAS_AUDIO_CODECS_BUILT_IN:
            self.assertEqual(skip_dict, {})
        else:
            self.assertTrue('ff_mp3_decoder' in skip_dict)
        self.assertFalse('WebGLShader' in skip_dict)

    def test_all_test_configurations(self):
        """Validate the complete set of configurations this port knows about."""
        port = self.make_port()
        self.assertEqual(set(port.all_test_configurations()), set([
            TestConfiguration('snowleopard', 'x86', 'debug'),
            TestConfiguration('snowleopard', 'x86', 'release'),
            TestConfiguration('lion', 'x86', 'debug'),
            TestConfiguration('lion', 'x86', 'release'),
            TestConfiguration('retina', 'x86', 'debug'),
            TestConfiguration('retina', 'x86', 'release'),
            TestConfiguration('mountainlion', 'x86', 'debug'),
            TestConfiguration('mountainlion', 'x86', 'release'),
            TestConfiguration('xp', 'x86', 'debug'),
            TestConfiguration('xp', 'x86', 'release'),
            TestConfiguration('win7', 'x86', 'debug'),
            TestConfiguration('win7', 'x86', 'release'),
            TestConfiguration('lucid', 'x86', 'debug'),
            TestConfiguration('lucid', 'x86', 'release'),
            TestConfiguration('lucid', 'x86_64', 'debug'),
            TestConfiguration('lucid', 'x86_64', 'release'),
            TestConfiguration('icecreamsandwich', 'x86', 'debug'),
            TestConfiguration('icecreamsandwich', 'x86', 'release'),
        ]))

    class TestMacPort(mac.MacPort):
        def __init__(self, options=None):
            options = options or MockOptions()
            mac.MacPort.__init__(self, MockSystemHost(os_name='mac', os_version='leopard'), 'mac-leopard', options=options)

        def default_configuration(self):
            self.default_configuration_called = True
            return 'default'

    class TestAndroidPort(android.AndroidPort):
        def __init__(self, options=None):
            options = options or MockOptions()
            android.AndroidPort.__init__(self, MockSystemHost(os_name='android', os_version='icecreamsandwich'), 'android', options=options)

        def default_configuration(self):
            self.default_configuration_called = True
            return 'default'

    class TestLinuxPort(linux.LinuxPort):
        def __init__(self, options=None):
            options = options or MockOptions()
            linux.LinuxPort.__init__(self, MockSystemHost(os_name='linux', os_version='lucid'), 'linux-x86', options=options)

        def default_configuration(self):
            self.default_configuration_called = True
            return 'default'

    class TestWinPort(win.WinPort):
        def __init__(self, options=None):
            options = options or MockOptions()
            win.WinPort.__init__(self, MockSystemHost(os_name='win', os_version='xp'), 'win-xp', options=options)

        def default_configuration(self):
            self.default_configuration_called = True
            return 'default'

    def test_default_configuration(self):
        mock_options = MockOptions()
        port = ChromiumPortTestCase.TestLinuxPort(options=mock_options)
        self.assertEqual(mock_options.configuration, 'default')  # pylint: disable=E1101
        self.assertTrue(port.default_configuration_called)

        mock_options = MockOptions(configuration=None)
        port = ChromiumPortTestCase.TestLinuxPort(mock_options)
        self.assertEqual(mock_options.configuration, 'default')  # pylint: disable=E1101
        self.assertTrue(port.default_configuration_called)

    def test_diff_image(self):
        class TestPort(ChromiumPortTestCase.TestLinuxPort):
            def _path_to_image_diff(self):
                return "/path/to/image_diff"

        port = ChromiumPortTestCase.TestLinuxPort()
        mock_image_diff = "MOCK Image Diff"

        def mock_run_command(args):
            port._filesystem.write_binary_file(args[4], mock_image_diff)
            return 1

        # Images are different.
        port._executive = MockExecutive2(run_command_fn=mock_run_command)
        self.assertEqual(mock_image_diff, port.diff_image("EXPECTED", "ACTUAL")[0])

        # Images are the same.
        port._executive = MockExecutive2(exit_code=0)
        self.assertEqual(None, port.diff_image("EXPECTED", "ACTUAL")[0])

        # There was some error running image_diff.
        port._executive = MockExecutive2(exit_code=2)
        exception_raised = False
        try:
            port.diff_image("EXPECTED", "ACTUAL")
        except ValueError, e:
            exception_raised = True
        self.assertFalse(exception_raised)

    def test_diff_image_crashed(self):
        port = ChromiumPortTestCase.TestLinuxPort()
        port._executive = MockExecutive2(exit_code=2)
        self.assertEqual(port.diff_image("EXPECTED", "ACTUAL"), (None, 'image diff returned an exit code of 2'))

    def test_expectations_files(self):
        port = self.make_port()
        port.port_name = 'chromium'

        generic_path = port.path_to_generic_test_expectations_file()
        chromium_overrides_path = port.path_from_chromium_base(
            'webkit', 'tools', 'layout_tests', 'test_expectations.txt')
        never_fix_tests_path = port._filesystem.join(port.layout_tests_dir(), 'NeverFixTests')
        slow_tests_path = port._filesystem.join(port.layout_tests_dir(), 'SlowTests')
        skia_overrides_path = port.path_from_chromium_base(
            'skia', 'skia_test_expectations.txt')

        port._filesystem.write_text_file(skia_overrides_path, 'dummay text')

        port._options.builder_name = 'DUMMY_BUILDER_NAME'
        self.assertEqual(port.expectations_files(), [generic_path, skia_overrides_path, never_fix_tests_path, slow_tests_path, chromium_overrides_path])

        port._options.builder_name = 'builder (deps)'
        self.assertEqual(port.expectations_files(), [generic_path, skia_overrides_path, never_fix_tests_path, slow_tests_path, chromium_overrides_path])

        # A builder which does NOT observe the Chromium test_expectations,
        # but still observes the Skia test_expectations...
        port._options.builder_name = 'builder'
        self.assertEqual(port.expectations_files(), [generic_path, skia_overrides_path, never_fix_tests_path, slow_tests_path])

    def test_expectations_ordering(self):
        # since we don't implement self.port_name in ChromiumPort.
        pass


class ChromiumPortLoggingTest(logtesting.LoggingTestCase):
    def test_check_sys_deps(self):
        port = ChromiumPortTestCase.TestLinuxPort()

        # Success
        port._executive = MockExecutive2(exit_code=0)
        self.assertTrue(port.check_sys_deps(needs_http=False))

        # Failure
        port._executive = MockExecutive2(exit_code=1,
            output='testing output failure')
        self.assertFalse(port.check_sys_deps(needs_http=False))
        self.assertLog([
            'ERROR: System dependencies check failed.\n',
            'ERROR: To override, invoke with --nocheck-sys-deps\n',
            'ERROR: \n',
            'ERROR: testing output failure\n'])

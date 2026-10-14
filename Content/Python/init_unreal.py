# Registers the Cinemafy toolset with the ToolsetRegistry so its tools are callable over unreal-mcp.
# Skipped quietly when the ToolsetRegistry plugin isn't enabled.
import unreal

try:
    import toolset_registry  # noqa: F401
except ImportError:
    toolset_registry = None

if toolset_registry is not None:
    from cinemafy_toolset import toolsets
    from cinemafy_toolset import tests

    toolsets._registration.register()

    tests._test_runner = unreal.PythonTestRunner.create(
        'AI.Toolsets.CinemafyTools',
        unreal.PythonTestRunnerSearchOptions(root_module=tests.__name__))

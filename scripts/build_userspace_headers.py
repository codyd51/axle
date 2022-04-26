import argparse
from pathlib import Path
import datetime
from typing import Dict
import shutil
from mesonbuild.build import Build
from mesonbuild.environment import Environment
from mesonbuild.interpreter.interpreterobjects import HeadersHolder
from mesonbuild.interpreter.primitives import StringHolder
from mesonbuild.interpreter.interpreter import Interpreter
from mesonbuild.mparser import FunctionNode, ArrayNode, StringNode, IdNode, AssignmentNode
from mesonbuild.interpreterbase.exceptions import InvalidCode
from typing import List
from tempfile import TemporaryDirectory
import pickle

from dataclasses import dataclass

from build_utils import second_file_is_older


@dataclass
class MoveHeaderToSysrootOperation:
    input_file: Path
    sysroot_file: Path


def generate_meson_cross_file_if_necessary() -> Path:
    # https://github.com/mesonbuild/meson/issues/309
    # Since Meson won't let us fill in the repo root with an environment variable, 
    # we have to template the file ourselves...
    programs_root = Path(__file__).parents[1] / "programs"
    cross_compile_config_path = programs_root / "cross_axle_generated.ini"
    cross_compile_config_template = programs_root / "cross_axle_template.ini"
    if not cross_compile_config_path.exists() or second_file_is_older(cross_compile_config_template, cross_compile_config_path):
        print(f'Generating cross_axle.ini...')
        if not cross_compile_config_template.exists():
            raise ValueError(f'Cross compile template file didn\'t exist!')
        cross_compile_config = cross_compile_config_template.read_text()
        cross_compile_config = f'[constants]\n' \
                               f'axle_repo_root = \'{Path(__file__).parents[1].as_posix()}\'\n' \
                               f'{cross_compile_config}'
        cross_compile_config_path.write_text(cross_compile_config)
    return cross_compile_config_path


def build_headers_from_meson_build_file(cross_file: Path, meson_file: Path) -> List[MoveHeaderToSysrootOperation]:
    move_operations = []
    with TemporaryDirectory() as dummy_build_directory:
        namespace = argparse.Namespace(cross_file=[cross_file], native_file=None, cmd_line_options=[], backend='ninja')
        env = Environment(source_dir=meson_file.parent.as_posix(), build_dir=dummy_build_directory, options=namespace)
        b = Build(env)
        int = Interpreter(b, user_defined_options=argparse.Namespace(vsenv=None))

        for line in int.ast.lines:
            if isinstance(line, FunctionNode) and line.func_name == 'install_headers':
                try:
                    headers_holder: HeadersHolder = int.evaluate_statement(line)
                except InvalidCode as e:
                    # There's probably a variable definition we need to evaluate
                    # missing_definition = 'headers_dir'
                    missing_definition = str(e).split("Unknown variable \"")[1].split("\".")[0]
                    for line2 in int.ast.lines:
                        if isinstance(line2, AssignmentNode) and line2.var_name == missing_definition:
                            # Make meson think we're executing a top-level statement again
                            int.argument_depth = 0
                            int.evaluate_statement(line2)
                            headers_holder: HeadersHolder = int.evaluate_statement(line)
                            break

                headers_description = headers_holder.held_object
                install_headers_dir = headers_description.custom_install_dir

                for header_file in headers_description.sources:
                    header_name = header_file.fname
                    absolute_header = meson_file.parent / header_name
                    move_operations.append(MoveHeaderToSysrootOperation(input_file=absolute_header, sysroot_file=Path(install_headers_dir) / header_name))
    return move_operations


def copy_userspace_headers() -> None:
    generate_meson_cross_file_if_necessary()
    programs_root = Path(__file__).parents[1] / "programs"
    # Read the top-level meson.build file
    root_build_path = programs_root / "meson.build"
    cross_file = programs_root / "cross_axle_generated.ini"
    namespace = argparse.Namespace(cross_file=[cross_file], native_file=None, cmd_line_options=[], backend='ninja')

    print('Parsing meson build files...')

    cache_file = Path(__file__).parent / "caches" / "header_rebuild_cache.dat"
    rebuild_dates: Dict[Path, datetime.datetime] = {}
    if cache_file.exists():
        rebuild_dates = pickle.loads(cache_file.read_bytes())

    subproject_to_move_operations = {}
    with TemporaryDirectory() as dummy_build_dir:
        env = Environment(source_dir=root_build_path.parent.as_posix(), build_dir=dummy_build_dir, options=namespace)
        b = Build(env)
        meson_interp = Interpreter(b, user_defined_options=argparse.Namespace(vsenv=None))

        for line in meson_interp.ast.lines:
            if isinstance(line, FunctionNode) and line.func_name == 'subproject':
                arg0 = line.args.arguments[0]
                assert isinstance(arg0, StringNode)
                subproject_name = arg0.value
                subproject_path = programs_root / "subprojects" / subproject_name

                should_rebuild = False
                if subproject_path not in rebuild_dates:
                    should_rebuild = True
                else:
                    # Check if any files have been updated since the last time we rebuilt its headers
                    last_rebuild_date = rebuild_dates[subproject_path]
                    for subfile in subproject_path.iterdir():
                        modify_date = datetime.datetime.fromtimestamp(subfile.lstat().st_mtime)
                        if modify_date > last_rebuild_date:
                            should_rebuild = True
                            break
                
                if should_rebuild:
                    print(f'Rebuilding subproject {subproject_name}')
                    rebuild_dates[subproject_path] = datetime.datetime.now()
                    build_file = subproject_path / 'meson.build'
                    move_operations = build_headers_from_meson_build_file(cross_file, build_file)
                    subproject_to_move_operations[subproject_name] = move_operations
    
    cache_file.mkdir(parents=True, exist_ok=True)
    cache_file.write_bytes(pickle.dumps(rebuild_dates))

    print()
    print()
    print('Executing parsed move operations...')
    for subproject, move_ops in subproject_to_move_operations.items():
        if not move_ops:
            continue
        print(f'{subproject}:')
        for move in move_ops:
            include_path = move.sysroot_file
            print(f'\t{move.input_file.as_posix()} -> {include_path.as_posix()}')
            include_path.parent.mkdir(exist_ok=True, parents=True)
            shutil.copy(move.input_file.as_posix(), include_path.as_posix())


if __name__ == "__main__":
    copy_userspace_headers()

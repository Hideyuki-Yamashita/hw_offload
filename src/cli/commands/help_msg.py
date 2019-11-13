# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2019 Nippon Telegraph and Telephone Corporation

cmds = {
        'status':
        """Display status info of SPP processes.

        spp > status
        """,

        'record':
        """Save commands as a recipe file.

        Save all of commands to a specified file as a recipe. This file
        is reloaded with 'playback' command later. You can also edit
        the recipe by hand to customize.

        spp > record path/to/recipe_file
        """,

        'playback':
        """Setup a network configuration from recipe file.

        Recipe is a file describing a series of SPP command to setup
        a network configuration.

        spp > playback path/to/recipe_file
        """,

        'config':
        """Show or update config.

        # show list of config
        spp > config

        # set prompt to "$ spp "
        spp > config prompt "$ spp "
        """,

        'pwd':
        """Show corrent directory.

        It behaves as UNIX's pwd command.

        spp > pwd
        """,

        'ls':
        """Show a list of specified directory.

        It behaves as UNIX's ls command.

        spp > ls path/to/dir
        """,

        'cd':
        """Change current directory.

        spp > cd path/to/dir
        """,

        'mkdir':
        """Create a new directory.

        It behaves as 'mkdir -p' which means that you can create sub
        directories at once.

        spp > mkdir path/to/dir
        """,

        'cat':
        """View contents of a file.

        spp > cat file
        """,

        'redo':
        """Execute command of index of history.

        spp > redo 5  # exec 5th command in the history
        """,

        'history':
        """Show command history.

        spp > history
          1  ls
          2  cat file.txt
          ...

        Command history is recorded in a file named '.spp_history'.
        It does not add some command which are no meaning for history.
        'bye', 'exit', 'history', 'redo'
        """,

        'less':
        """View contents of a file.

        spp > less file
        """,

        'exit':
        """Terminate SPP controller process.

        It is an alias of bye command to terminate controller.

        spp > exit
        Thank you for using Soft Patch Panel
        """,

        'inspect':
        """Print attributes of Shell for debugging.

        This command is intended to be used by developers to show the
        inside of the object of Shell class.

        spp > inspect
        {'cmdqueue': [],
         'completekey': 'tab',
         'completion_matches': ['inspect'],
         ...
        """,

        'load_cmd':
        """Load command plugin.

        Path of plugin file is 'spp/src/controller/plugins'.

        spp > load_cmd hello
        """,
        }

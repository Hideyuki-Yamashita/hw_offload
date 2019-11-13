# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Nippon Telegraph and Telephone Corporation


class SppBye(object):
    """Run SPP bye command.

    SppBye class is intended to be used in Shell class as a delegator
    for running 'bye' command.

    'self.command()' is called from do_bye() and 'self.complete()' is called
    from complete_bye() of both of which is defined in Shell.
    """

    BYE_CMDS = ['sec', 'all']

    def __init__(self):
        pass

    def run(self, args, spp_primary, spp_secondaries):

        cmds = args.split(' ')
        if cmds[0] == 'sec':
            print('Closing secondary ...')
            self.close_all_secondary(spp_secondaries)

        elif cmds[0] == 'all':
            print('Closing secondary ...')
            self.close_all_secondary(spp_secondaries)
            print('Closing primary ...')
            spp_primary.do_exit()

    def complete(self, text, line, begidx, endidx):

        if not text:
            completions = self.BYE_CMDS[:]
        else:
            completions = [p
                           for p in self.BYE_CMDS
                           if p.startswith(text)
                           ]
        return completions

    def close_all_secondary(self, spp_secondaries):
        """Terminate all secondary processes."""

        for sec_type, spp_procs in spp_secondaries.items():
            for sec in spp_procs.values():
                sec.run('exit')

    @classmethod
    def help(cls):
        msg = """Terminate SPP processes and controller.

        There are three usages for terminating processes.
        It terminates logging if you activated recording.

        (1) Terminate secondary processes
            spp > bye sec

        (2) Terminate primary and secondary processes
            spp > bye all

        (3) Terminate SPP controller (not for primary and secondary)
            spp > bye
        """

        print(msg)

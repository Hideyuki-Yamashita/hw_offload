# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017-2018 Nippon Telegraph and Telephone Corporation

import os
import re
import socket
import subprocess
import sys
import traceback
import uuid
import yaml
from .. import spp_common
from ..spp_common import logger


class SppTopo(object):
    """Setup and output network topology for topo command

    Topo command supports four types of output.
    * terminal (but very few terminals supporting to display images)
    * browser (websocket server is required)
    * image file (jpg, png, bmp)
    * text (dot, json, yaml)
    """

    def __init__(self, spp_ctl_cli, subgraphs, cli_config):
        self.spp_ctl_cli = spp_ctl_cli
        self.subgraphs = subgraphs
        self.graph_size = None

        # Graphviz params
        topo_file = '{dir}/../config/topo.yml'.format(
                dir=os.path.dirname(__file__))
        topo_conf = yaml.load(open(topo_file), Loader=yaml.FullLoader)
        self.SEC_COLORS = topo_conf['topo_sec_colors']['val']
        self.PORT_COLORS = topo_conf['topo_port_colors']['val']
        self.LINE_STYLE = {"running": "solid", "idling": "dashed"}
        self.GRAPH_TYPE = "digraph"
        self.LINK_TYPE = "->"
        self.SPP_PCAP_LABEL = 'spppcap'  # Label of dummy port of spp_pcap
        self.delim_node = '_'  # used for node ID such as 'phy_0' or 'ring_1'
        self.node_temp = '{}' + self.delim_node + '{}'  # template of node ID

        # topo should support spp_pcap's file and tap port
        self.all_port_types = spp_common.PORT_TYPES + [
                self.SPP_PCAP_LABEL, 'tap']

        # Add colors for custom ports
        self.PORT_COLORS[self.SPP_PCAP_LABEL] = "gold2"

        if self.resize_graph(cli_config['topo_size']['val']) is not True:
            print('Config "topo_size" is invalid value.')
            exit()

    def run(self, args):
        args_ary = args.split()
        if len(args_ary) == 0:
            print("Usage: topo dst [ftype]")
            return False
        elif args_ary[0] == "term":
            self.to_term(self.graph_size)
        elif args_ary[0] == "http":
            self.to_http()
        elif len(args_ary) == 1:  # find ftype from filename
            ftype = args_ary[0].split(".")[-1]
            self.to_file(args_ary[0], ftype)
        elif len(args_ary) == 2:  # ftype is given as args_ary[1]
            self.to_file(args_ary[0], args_ary[1])
        else:
            print("Usage: topo dst [ftype]")

    def resize_graph(self, size):
        """Parse given size and set to self.graph_size.

        The format of `size` is percentage or ratio. Return True if succeeded
        to parse, or False if invalid format.
        """

        size = str(size)
        matched = re.match(r'(\d+)%$', size)
        if matched:  # percentage
            i = int(matched.group(1))
            if i > 0 and i <= 100:
                self.graph_size = size
                return True
            else:
                return False
        elif re.match(r'0\.\d+$', size):  # ratio
            i = float(size) * 100
            self.graph_size = str(i) + '%'
            return True
        elif size == '1':
            self.graph_size = '100%'
            return True
        else:
            return False

    def to_file(self, fname, ftype="dot"):
        if ftype == "dot":
            if self.to_dot(fname) is not True:
                return False
        elif ftype == "json" or ftype == "js":
            self.to_json(fname)
        elif ftype == "yaml" or ftype == "yml":
            self.to_yaml(fname)
        elif ftype == "jpg" or ftype == "png" or ftype == "bmp":
            if self.to_img(fname) is not True:
                return False
        else:
            print("Invalid file type")
            return False

        print("Create topology: '{fname}'".format(fname=fname))
        return True

    def to_dot(self, output_fname):
        """Output dot script."""

        node_attrs = 'node[shape="rectangle", style="filled"];'

        ports, links = self._get_dot_elements()

        # Check if one or more ports exist.
        port_cnt = 0
        for val in ports.values():
            port_cnt += len(val)
        if port_cnt == 0:
            print('No secondary process exist!')
            return False

        # Remove duplicated entries.
        for ptype in spp_common.PORT_TYPES:
            ports[ptype] = list(set(ports[ptype]))

        output = ["{} spp{{".format(self.GRAPH_TYPE)]
        output.append("newrank=true;")
        output.append(node_attrs)

        # Setup node entries
        nodes = {}
        for ptype in self.all_port_types:
            nodes[ptype] = []
            for node in ports[ptype]:
                r_type, r_id = node.split(':')
                nodes[ptype].append(
                    self.node_temp.format(r_type, r_id))
            nodes[ptype] = list(set(nodes[ptype]))
            for node in nodes[ptype]:
                label = re.sub(r'{}'.format(self.delim_node), ':', node)
                output.append(
                    '{nd}[label="{lbl}", fillcolor="{col}"];'.format(
                        nd=node, lbl=label, col=self.PORT_COLORS[ptype]))

        # Align the same type of nodes with rank attribute
        for ptype in self.all_port_types:
            if len(nodes[ptype]) > 0:
                output.append(
                    '{{rank=same; {}}}'.format("; ".join(nodes[ptype])))

        # Decide the bottom, phy or vhost
        # TODO(yasufum) revise how to decide bottom
        rank_style = '{{rank=max; ' + self.node_temp + '}}'
        if len(ports['phy']) > 0:
            r_type, r_id = ports['phy'][0].split(':')
        elif len(ports['vhost']) > 0:
            r_type, r_id = ports['vhost'][0].split(':')
        output.append(rank_style.format(r_type, r_id))

        # Add subgraph
        ssgs = []
        if len(self.subgraphs) > 0:
            cnt = 1
            for label, val in self.subgraphs.items():
                cluster_id = "cluster{}".format(cnt)
                ssg_label = label
                ssg_ports = re.sub(r'%s' % ':', self.delim_node, val)
                ssg = 'subgraph {cid} {{label="{ssgl}" {ssgp}}}'.format(
                    cid=cluster_id, ssgl=ssg_label, ssgp=ssg_ports)
                ssgs.append(ssg)
                cnt += 1

        # Setup ports included in Host subgraph
        host_nodes = []
        for ptype in self.all_port_types:
            host_nodes = host_nodes + nodes[ptype]

        cluster_id = "cluster0"
        sg_label = "Host"
        sg_ports = "; ".join(host_nodes)
        if len(ssgs) == 0:
            output.append(
                'subgraph {cid} {{label="{sgl}" {sgp}}}'.format(
                    cid=cluster_id, sgl=sg_label, sgp=sg_ports))
        else:
            tmp = 'label="{sgl}" {sgp}'.format(sgl=sg_label, sgp=sg_ports)
            contents = [tmp] + ssgs
            output.append(
                'subgraph {cid} {{{cont}}}'.format(
                    cid=cluster_id, cont='; '.join(contents)))

        # Add links
        for link in links:
            output.append(link)

        output.append("}")

        f = open(output_fname, "w+")
        f.write("\n".join(output))
        f.close()

        return True

    def to_json(self, output_fname):
        import json
        f = open(output_fname, "w+")
        sec_list = self.spp_ctl_cli.get_sec_procs('nfv')
        f.write(json.dumps(sec_list))
        f.close()

    def to_yaml(self, output_fname):
        import yaml
        f = open(output_fname, "w+")
        sec_list = self.spp_ctl_cli.get_sec_procs('nfv')
        f.write(yaml.dump(sec_list))
        f.close()

    def to_img(self, output_fname):
        tmpfile = "{fn}.dot".format(fn=uuid.uuid4().hex)
        if self.to_dot(tmpfile) is not True:
            return False

        fmt = output_fname.split(".")[-1]
        cmd = "dot -T{fmt} {dotf} -o {of}".format(
                fmt=fmt, dotf=tmpfile, of=output_fname)
        subprocess.call(cmd, shell=True)
        subprocess.call("rm -f {tmpf}".format(tmpf=tmpfile), shell=True)

        return True

    def to_http(self):
        import websocket
        tmpfile = "{fn}.dot".format(fn=uuid.uuid4().hex)
        if self.to_dot(tmpfile) is not True:
            return False

        msg = open(tmpfile).read()
        subprocess.call("rm -f {tmpf}".format(tmpf=tmpfile), shell=True)
        # TODO(yasufum) change to be able to use other than `localhost`.
        ws_url = "ws://localhost:8989/spp_ws"
        try:
            ws = websocket.create_connection(ws_url)
            ws.send(msg)
            ws.close()
        except socket.error:
            print('Error: Connection refused! Is running websocket server?')

        return True

    def to_term(self, size):
        tmpfile = "{fn}.jpg".format(fn=uuid.uuid4().hex)
        if self.to_img(tmpfile) is not True:
            return False

        from distutils import spawn

        # TODO(yasufum) add check for using only supported terminal
        if spawn.find_executable("img2sixel") is not None:
            img_cmd = "img2sixel"
        else:
            imgcat = "{pdir}/{sdir}/imgcat".format(
                pdir=os.path.dirname(__file__), sdir='3rd_party')
            if os.path.exists(imgcat) is True:
                img_cmd = imgcat
            else:
                img_cmd = None

        if img_cmd is not None:
            # Resize image to fit the terminal
            img_size = size
            cmd = "convert -resize {size} {fn1} {fn2}".format(
                    size=img_size, fn1=tmpfile, fn2=tmpfile)
            subprocess.call(cmd, shell=True)
            subprocess.call("{cmd} {fn}".format(cmd=img_cmd, fn=tmpfile),
                            shell=True)
            subprocess.call(["rm", "-f", tmpfile])
        else:
            print("img2sixel (or imgcat.sh for MacOS) not found!")
            topo_doc = "https://spp.readthedocs.io/en/latest/"
            topo_doc += "commands/experimental.html"
            print("See '{url}' for required packages.".format(url=topo_doc))

    def format_sec_status(self, sec_id, stat):
        """Return formatted secondary status as a hash

        By running status command on controller, status is sent from
        secondary process and receiving message is displayed.

        This is an example of receiving status message.

        spp > sec 1;status
        status: idling
        ports:
          - 'phy:0 -> vhost:1'
          - 'phy:1'
          - 'ring:0'
          - 'vhost:1 -> ring:0'

        This method returns a result as following.
        {
            'sec_id': '1',
            'status': 'idling',
            'ports': [
                 {
                     'rid': 'phy:0',
                     'out': 'vhost:0',
                     'iface': {'type': 'phy', 'id': '0'}
                 },
                 {
                     'rid': 'phy:1',
                     'out': None,
                     'iface': {'type': 'phy', 'id': '1'}
                 }
            ],
            'sec_id': '2',
            ...
        }
        """

        # Clean sec stat message
        stat = stat.replace("\x00", "")
        stat = stat.replace("'", "")

        stat_obj = yaml.load(stat, Loader=yaml.FullLoader)
        res = {}
        res['sec_id'] = sec_id
        res['status'] = stat_obj['status']

        port_list = []
        try:
            if stat_obj['ports'] is None:
                return None

            ports = stat_obj['ports'].split(',')
            for port_info in ports:
                rid, outport = port_info.split('-')
                if outport == 'null':
                    outport = None

                itype, pid = rid.split(':')
                iface = {'type': itype, 'id': pid}
                port_list.append({
                    'rid': rid,
                    'out': outport,
                    'iface': iface
                })

            res["ports"] = port_list
            return res

        except Exception:
            traceback.print_exc()
            return None

    def complete(self, text, line, begidx, endidx):
        """Complete topo command

        If no token given, return 'term' and 'http'.
        On the other hand, complete 'term' or 'http' if token starts
        from it, or complete file name if is one of supported formats.
        """

        terms = ['term', 'http']
        # Supported formats
        img_exts = ['jpg', 'png', 'bmp']
        txt_exts = ['dot', 'yml', 'js']

        # Number of given tokens is expected as two. First one is
        # 'topo'. If it is three or more, this methods returns nothing.
        tokens = re.sub(r"\s+", " ", line).split(' ')
        if (len(tokens) == 2):
            if (text == ''):
                completions = terms
            else:
                completions = []
                # Check if 2nd token is a part of terms.
                for t in terms:
                    if t.startswith(tokens[1]):
                        completions.append(t)
                # Not a part of terms, so check for completion for
                # output file name.
                if len(completions) == 0:
                    if tokens[1].endswith('.'):
                        completions = img_exts + txt_exts
                    elif ('.' in tokens[1]):
                        fname = tokens[1].split('.')[0]
                        token = tokens[1].split('.')[-1]
                        for ext in img_exts:
                            if ext.startswith(token):
                                completions.append(fname + '.' + ext)
                        for ext in txt_exts:
                            if ext.startswith(token):
                                completions.append(fname + '.' + ext)
            return completions
        else:  # do nothing for three or more tokens
            pass

    def _is_valid_port(self, port):
        """Check if port's format is valid.

        Return True if the format is 'r_type:r_id', for example, 'phy:0'.
        """

        if (':' in port) and (len(port.split(':')) == 2):
            return True
        else:
            return False

    def _is_comp_running(self, comp):
        """Find out given comp is running or not for spp_vf or spp_mirror.

        Each of component has its condition for starting packet processing.
        Return True if it is already running, or False if not.
        """

        # TODO(yasufum) implement it.
        if comp['type'] == 'forward':  # TODO change to forwarder
            pass
        if comp['type'] == 'classifier':
            pass
        if comp['type'] == 'merge':  # TODO change to merger
            pass
        elif comp['type'] == 'mirror':
            pass
        return True

    def _get_dot_elements(self):
        """Get entries of nodes and links.

        To generate dot script, this method returns ports as nodes and links
        which are used as a part of element in dot language.
        """

        ports = {}
        links = []

        # Initialize ports
        for ptype in self.all_port_types:
            ports[ptype] = []

        # parse status message from sec.
        for proc_t in ['nfv', 'vf', 'mirror', 'pcap']:
            for sec in self.spp_ctl_cli.get_sec_procs(proc_t):
                if sec is None:
                    continue
                self._setup_dot_ports(ports, sec, proc_t)
                self._setup_dot_links(links, sec, proc_t)

        return ports, links

    def _setup_dot_ports(self, ports, sec, proc_t):
        """Parse sec obj and append port to `ports`."""

        try:
            if proc_t in ['nfv', 'vf', 'mirror']:
                for port in sec['ports']:
                    if self._is_valid_port(port):
                        r_type = port.split(':')[0]
                        if r_type in self.all_port_types:
                            ports[r_type].append(port)
                        else:
                            raise ValueError(
                                "Invaid interface type: {}".format(r_type))

            elif proc_t == 'pcap':
                for c in sec['core']:
                    if c['role'] == 'receive':
                        if len(c['rx_port']) > 0:
                            port = c['rx_port'][0]['port']
                            if self._is_valid_port(port):
                                r_type = port.split(':')[0]
                                if r_type in self.all_port_types:
                                    ports[r_type].append(port)
                                else:
                                    raise ValueError(
                                        "Invaid interface type: {}".format(
                                            r_type))
                        else:
                            print('Error: No rx port in {}:{}.'.format(
                                'pcap', sec['client-id']))
                            return False
                ports[self.SPP_PCAP_LABEL].append(
                        '{}:{}'.format(self.SPP_PCAP_LABEL, sec['client-id']))

            else:
                logger.error('Invlaid secondary type {}.'.format(proc_t))

            return True

        except IndexError as e:
            print('Error: Failed to parse ports. ' + e)
        except KeyError as e:
            print('Error: Failed to parse ports. ' + e)

    def _setup_dot_links(self, links, sec, proc_t):
        """Parse sec obj and append links to `links`."""

        link_style = self.node_temp + ' {} ' + self.node_temp + '{};'
        try:
            # Get links
            src_type, src_id, dst_type, dst_id = None, None, None, None
            if proc_t == 'nfv':
                for patch in sec['patches']:
                    if sec['status'] == 'running':
                        l_style = self.LINE_STYLE["running"]
                    else:
                        l_style = self.LINE_STYLE["idling"]
                    attrs = '[label="{}", color="{}", style="{}"]'.format(
                            "nfv:{}".format(sec["client-id"]),
                            self.SEC_COLORS[sec["client-id"]],
                            l_style)

                    if self._is_valid_port(patch['src']):
                        src_type, src_id = patch['src'].split(':')
                    if self._is_valid_port(patch['dst']):
                        dst_type, dst_id = patch['dst'].split(':')

                    if src_type is None or dst_type is None:
                        print('Error: Failed to parse links in {}:{}.'.format(
                            'nfv', sec['client-id']))
                        return False

                    tmp = link_style.format(src_type, src_id,
                                            self.LINK_TYPE,
                                            dst_type, dst_id, attrs)
                    links.append(tmp)

            elif proc_t == 'vf':
                for comp in sec['components']:
                    if self._is_comp_running(comp):
                        l_style = self.LINE_STYLE["running"]
                    else:
                        l_style = self.LINE_STYLE["idling"]
                    attrs = '[label="{}", color="{}", style="{}"]'.format(
                            "vf:{}:{}".format(
                                sec["client-id"], comp['type'][0]),
                            self.SEC_COLORS[sec["client-id"]],
                            l_style)

                    if comp['type'] == 'forward':
                        if len(comp['rx_port']) > 0:
                            rxport = comp['rx_port'][0]['port']
                            if self._is_valid_port(rxport):
                                src_type, src_id = rxport.split(':')
                        if len(comp['tx_port']) > 0:
                            txport = comp['tx_port'][0]['port']
                            if self._is_valid_port(txport):
                                dst_type, dst_id = txport.split(':')

                        if src_type is None or dst_type is None:
                            print('Error: {msg} {comp}:{sid} {ct}'.format(
                                msg='Falied to parse links in', comp='vf',
                                sid=sec['client-id'], ct=comp['type']))
                            return False

                        tmp = link_style.format(src_type, src_id,
                                                self.LINK_TYPE,
                                                dst_type, dst_id, attrs)
                        links.append(tmp)

                    elif comp['type'] == 'classifier':
                        if len(comp['rx_port']) > 0:
                            rxport = comp['rx_port'][0]['port']
                            if self._is_valid_port(rxport):
                                src_type, src_id = rxport.split(':')
                        for txp in comp['tx_port']:
                            if self._is_valid_port(txp['port']):
                                dst_type, dst_id = txp['port'].split(':')

                            if src_type is None or dst_type is None:
                                print('Error: {msg} {comp}:{sid} {ct}'.format(
                                    msg='Falied to parse links in', comp='vf',
                                    sid=sec['client-id'], ct=comp['type']))
                                return False

                            tmp = link_style.format(src_type, src_id,
                                                    self.LINK_TYPE,
                                                    dst_type, dst_id, attrs)
                            links.append(tmp)

                    elif comp['type'] == 'merge':  # TODO change to merger
                        if len(comp['tx_port']) > 0:
                            txport = comp['tx_port'][0]['port']
                            if self._is_valid_port(txport):
                                dst_type, dst_id = txport.split(':')
                        for rxp in comp['rx_port']:
                            if self._is_valid_port(rxp['port']):
                                src_type, src_id = rxp['port'].split(':')

                            if src_type is None or dst_type is None:
                                print('Error: {msg} {comp}:{sid} {ct}'.format(
                                    msg='Falied to parse links in', comp='vf',
                                    sid=sec['client-id'], ct=comp['type']))
                                return False

                            tmp = link_style.format(src_type, src_id,
                                                    self.LINK_TYPE,
                                                    dst_type, dst_id, attrs)
                            links.append(tmp)

            elif proc_t == 'mirror':
                for comp in sec['components']:
                    if self._is_comp_running(comp):
                        l_style = self.LINE_STYLE["running"]
                    else:
                        l_style = self.LINE_STYLE["idling"]
                    attrs = '[label="{}", color="{}", style="{}"]'.format(
                            "vf:{}".format(sec["client-id"]),
                            self.SEC_COLORS[sec["client-id"]],
                            l_style)

                    if len(comp['rx_port']) > 0:
                        rxport = comp['rx_port'][0]['port']
                        if self._is_valid_port(rxport):
                            src_type, src_id = rxport.split(':')
                    for txp in comp['tx_port']:
                        if self._is_valid_port(txp['port']):
                            dst_type, dst_id = txp['port'].split(':')

                        if src_type is None or dst_type is None:
                            print('Error: {msg} {comp}:{sid} {ct}'.format(
                                msg='Falied to parse links in', comp='vf',
                                sid=sec['client-id'], ct=comp['type']))
                            return False

                        tmp = link_style.format(src_type, src_id,
                                                self.LINK_TYPE,
                                                dst_type, dst_id, attrs)
                        links.append(tmp)

            elif proc_t == 'pcap':
                if sec['status'] == 'running':
                    l_style = self.LINE_STYLE["running"]
                else:
                    l_style = self.LINE_STYLE["idling"]
                attrs = '[label="{}", color="{}", style="{}"]'.format(
                        "pcap:{}".format(sec["client-id"]),
                        self.SEC_COLORS[sec["client-id"]], l_style)

                for c in sec['core']:  # TODO consider change to component
                    if c['role'] == 'receive':  # TODO change to receiver
                        if len(comp['rx_port']) > 0:
                            rxport = c['rx_port'][0]['port']
                            if self._is_valid_port(rxport):
                                src_type, src_id = rxport.split(':')
                        dst_type = self.SPP_PCAP_LABEL
                        dst_id = sec['client-id']
                tmp = link_style.format(src_type, src_id, self.LINK_TYPE,
                                        dst_type, dst_id, attrs)
                links.append(tmp)

            else:
                logger.error('Invlaid secondary type {}.'.format(proc_t))

            return True

        except IndexError as e:
            print('Error: Failed to parse links. "{}"'.format(e))
        except KeyError as e:
            print('Error: Failed to parse links. "{}"'.format(e))

    @classmethod
    def help(cls):
        msg = """Output network topology.

        Support four types of output.
        * terminal (but very few terminals supporting to display images)
        * browser (websocket server is required)
        * image file (jpg, png, bmp)
        * text (dot, js or json, yml or yaml)

        spp > topo term  # terminal
        spp > topo http  # browser
        spp > topo network_conf.jpg  # image
        spp > topo network_conf.dot  # text
        spp > topo network_conf.js# text
        """

        print(msg)

    @classmethod
    def help_subgraph(cls):
        msg = """Edit subgarph for topo command.

        Subgraph is a group of object defined in dot language. For topo
        command, it is used for grouping resources of each of VM or
        container to topology be more understandable.

        (1) Add subgraph labeled 'vm1'.
            spp > topo_subgraph add vm1 vhost:1;vhost:2

        (2) Delete subgraph 'vm1'.
            spp > topo_subgraph del vm1

        (3) Show subgraphs by running topo_subgraph without args.
            spp > topo_subgraph
            label: vm1	subgraph: "vhost:1;vhost:2"
        """

        print(msg)

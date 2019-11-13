#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017-2018 Nippon Telegraph and Telephone Corporation

import logging
import os
import tornado.escape
import tornado.ioloop
import tornado.options
import tornado.web
import tornado.websocket

from tornado.options import define
from tornado.options import options

curdir = os.path.abspath(os.path.dirname(__file__))
logging.basicConfig(
    filename='%s/server.log' % curdir, level=logging.DEBUG)

# Server address
ipaddr = '127.0.0.1'  # used only for start message
port = 8989

define("port", default=port, help="Wait for 'topo http' command", type=int)


class Application(tornado.web.Application):
    def __init__(self):
        handlers = [
            (r"/", MainHandler),
            (r"/spp_ws", SppWsHandler),
        ]
        settings = dict(
            template_path=os.path.join(
                os.path.dirname(__file__), "templates"),
            static_path=os.path.join(
                os.path.dirname(__file__), "static"),
            cookie_secret="yasupyasup",
            xsrf_cookies=True,
        )
        super(Application, self).__init__(handlers, **settings)


class MainHandler(tornado.web.RequestHandler):
    def get(self):
        logging.info("Render to browser\n%s", SppWsHandler.dot)
        self.render(
            "index.html",
            dotData=SppWsHandler.dot.replace('\n', ''))


class SppWsHandler(tornado.websocket.WebSocketHandler):
    waiters = set()
    dot = ""

    def get_compression_options(self):
        # Non-None enables compression with default options.
        return {}

    def open(self):
        SppWsHandler.waiters.add(self)

    def on_close(self):
        SppWsHandler.waiters.remove(self)

    @classmethod
    def update_dot(cls, msg):
        cls.dot = msg

    @classmethod
    def send_updates(cls, msg):
        logging.info(
            "Send message to %d clients", len(cls.waiters))
        for waiter in cls.waiters:
            try:
                waiter.write_message(msg)
            except Exception:
                pass
                logging.error("Error sending message", exc_info=True)

    def on_message(self, message):
        logging.info("Received from client\n%r", message)
        self.dot = message
        SppWsHandler.update_dot(message)
        SppWsHandler.send_updates(message)


def main():
    tornado.options.parse_command_line()
    app = Application()
    app.listen(options.port)
    print('Running SPP topo server on http://%s:%d ...' % (
        ipaddr, port))
    tornado.ioloop.IOLoop.current().start()


if __name__ == "__main__":
    main()

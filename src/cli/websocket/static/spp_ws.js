/**
 * @license
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2018 Nippon Telegraph and Telephone Corporation
 */

$(document).ready(function() {
  if (!window.console) window.console = {};
  if (!window.console.log) window.console.log = function() {};

  updater.start();
});

var updater = {
  socket: null,

  start: function() {
    var url = "ws://" + location.host + "/spp_ws";
    updater.socket = new WebSocket(url);
    updater.socket.onmessage = function(event) {
      updater.showDot(event.data);
    }
  },

  showDot: function(dot) {
    console.log(dot);
    dotData = dot
    draw();
  }
};


// create a network
var container = document.getElementById('spp_topo_network');
var options = {
  physics: {
    stabilization: false,
    barnesHut: {
      springLength: 200
    }
  }
};
var data = {};
var network = new vis.Network(container, data, options);

$(window).resize(resize);
$(window).load(draw);

function resize() {
  $('#contents').height($('body').height() - $('#header').height() - 30);
}

function draw() {
  try {
    resize();
    $('#error').html('');

    data = vis.network.convertDot(dotData);

    network.setData(data);
  }
  catch (err) {
    // show an error message
    $('#error').html(err.toString());
  }
}

window.onload = function() {
  draw();
}

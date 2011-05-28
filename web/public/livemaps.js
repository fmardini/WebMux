/*jslint white: true, onevar: false, undef: true, nomen: true, newcap: true, browser: true, maxerr: 50, indent: 2, nomen: false */
/*global WebSocket, setInterval, liveMaps:true, window, $, _, console, google */

var App = function (opts) {
  this.map = null;
  this.infoWindows = [];
  this.options = $.extend({
    lat: 31.00,
    lng: 36.00,
    zoom: 6,
    mapType: google.maps.MapTypeId.TERRAIN
  }, opts);

  this.setupMap = function () {
    var latLng = new google.maps.LatLng(this.options.lat, this.options.lng);
    var opts = {
      zoom: this.options.zoom,
      center: latLng,
      mapTypeId: this.options.mapType
    };
    this.map = new google.maps.Map($("#mapCanvas")[0], opts);
  };

  this.connect = function () {
    var socket = new WebSocket('ws://game.example.com:12010/updates');
    socket.onopen = function () {};
  };

  this.addInfoWindow = function (pos) {
    var iw = new google.maps.InfoWindow({position: pos, content: "<h2>CONTENT</h2>"});
    this.infoWindows.push(iw);
    iw.open(this.map);
  };
};

$(document).ready(function () {
  liveMaps = new App({});
  liveMaps.setupMap();
});

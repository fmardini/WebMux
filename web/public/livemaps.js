$(document).ready(function () {
  var latlng = new google.maps.LatLng(-34.397, 150.644);
  var myOptions = {
    zoom      : 8,
    center    : latlng,
    mapTypeId : google.maps.MapTypeId.ROADMAP
  };
  var map = new google.maps.Map($("#mapCanvas")[0], myOptions);
});
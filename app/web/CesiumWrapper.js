class CesiumWrapper {
    constructor(container, viewportController) {
        // Your access token can be found at: https://cesium.com/ion/tokens.
        // Replace `your_access_token` with your Cesium ion access token.
        // --> Cesium.Ion.defaultAccessToken = 'your_access_token';

        // Initialize the Cesium Viewer in the HTML element with the `cesiumContainer` ID.
        const viewer = new Cesium.Viewer(container, { terrainProvider: Cesium.createWorldTerrain() });
        // Add Cesium OSM Buildings, a global 3D buildings layer.
        const buildingTileset = viewer.scene.primitives.add(Cesium.createOsmBuildings());

        // signals
        viewportController.flyTo.connect(function(latitude, longitude, height, heading, pitch, duration) {
            viewer.camera.flyTo({
                         destination : Cesium.Cartesian3.fromDegrees(longitude, latitude, height),
                         orientation : {
                             heading : Cesium.Math.toRadians(heading),
                             pitch : Cesium.Math.toRadians(pitch),
                             roll : 0
                         },
                         duration: duration
                     });
        });

        viewportController.lookTo.connect(function(heading, pitch, duration) {
            viewer.camera.flyTo({
                         destination : viewer.camera.positionWC,
                         orientation : {
                             heading : Cesium.Math.toRadians(heading),
                             pitch : Cesium.Math.toRadians(pitch),
                             roll : viewer.camera.roll
                         },
                         duration: duration
                     });
        });

        var geodesic = new Cesium.EllipsoidGeodesic();

        // event listners
        viewer.scene.canvas.addEventListener('mousemove', function(event) {
            var ellipsoid = viewer.scene.globe.ellipsoid;
            // Mouse over the globe to see the cartographic position
            var cartesian = viewer.camera.pickEllipsoid(
                        new Cesium.Cartesian3(event.clientX, event.clientY), ellipsoid);
            if (cartesian) {
                var cartographic = ellipsoid.cartesianToCartographic(cartesian);

                viewportController.cursorPosition.latitude = Cesium.Math.toDegrees(cartographic.latitude);
                viewportController.cursorPosition.longitude = Cesium.Math.toDegrees(cartographic.longitude);
                // viewportController.cursorHeight = cartographic.height;
            } else {
                viewportController.cursorPosition.invalidate();
            }

            // Find the distance between two pixels at the bottom center of the screen.
            var width = viewer.scene.canvas.clientWidth;
            var height = viewer.scene.canvas.clientHeight;

            var left = viewer.camera.getPickRay(new Cesium.Cartesian2((width / 2) | 0, height - 1));
            var right = viewer.camera.getPickRay(new Cesium.Cartesian2(1 + (width / 2) | 0, height - 1));

            var globe = viewer.scene.globe;
            var leftPosition = globe.pick(left, viewer.scene);
            var rightPosition = globe.pick(right, viewer.scene);

            if (!(leftPosition) || !(rightPosition)) {
                viewportController.metersInPixel = 0;
                return;
            }

            var leftCartographic = globe.ellipsoid.cartesianToCartographic(leftPosition);
            var rightCartographic = globe.ellipsoid.cartesianToCartographic(rightPosition);

            geodesic.setEndPoints(leftCartographic, rightCartographic);
            var pixelDistance = geodesic.surfaceDistance;

            viewportController.metersInPixel = pixelDistance;
        });

        viewer.scene.postRender.addEventListener(function(){
          viewportController.heading = Cesium.Math.toDegrees(viewer.camera.heading);
          viewportController.pitch = Cesium.Math.toDegrees(viewer.camera.pitch);
        })
    }
}

const webChannel = new QWebChannel(qt.webChannelTransport, function(channel) {
    const viewportController = channel.objects.viewportController;
    const cesium = new CesiumWrapper('cesiumContainer', viewportController);
});

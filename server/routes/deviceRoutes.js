const express = require('express');

function createDeviceRouter({ pageController }) {
  const router = express.Router();

  router.get('/', pageController.renderHome);
  router.get('/device/:deviceKey', pageController.renderDeviceDetail);

  return router;
}

module.exports = {
  createDeviceRouter
};

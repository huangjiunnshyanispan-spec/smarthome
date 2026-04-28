const path = require('path');
const { DEVICE_VIEWS } = require('../config/deviceConfig');

function createPageController({ deviceModel }) {
  function renderHome(req, res) {
    res.sendFile(path.join(__dirname, '..', 'public', 'index.html'));
  }

  function renderDeviceDetail(req, res) {
    const { deviceKey } = req.params;
    const viewName = DEVICE_VIEWS[deviceKey];

    if (!viewName) {
      return res.status(404).send('找不到對應的裝置詳情頁');
    }

    return res.render(viewName, {
      signal: deviceModel.getSignal(deviceKey),
      deviceKey
    });
  }

  return {
    renderHome,
    renderDeviceDetail
  };
}

module.exports = {
  createPageController
};

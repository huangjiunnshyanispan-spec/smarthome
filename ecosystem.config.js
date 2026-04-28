module.exports = {
  apps: [
    {
      name: "server",
      script: "/home/pi/server/server.js",
      cwd: "/home/pi/server",
      autorestart: true
    },
    {
      name: "speaker",
      script: "/home/pi/speaker_agent/speaker_agent",
      cwd: "/home/pi/speaker_agent",
      // 等待 25 秒，確保 /etc/rc.local 跑完且讀條結束
      restart_delay: 25000, 
      autorestart: true
    },
    {
      name: "led-monitor",
      script: "/home/pi/led_monitor/led_monitor",
      cwd: "/home/pi/led_monitor",
      restart_delay: 28000, 
      autorestart: true
    }
  ]
};

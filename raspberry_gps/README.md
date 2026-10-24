1. create symlink /etc/systemd/system/gps_data.service to the gps_data.service file in the repo folder
2. ensure all paths are correct in the main.py script and in gps_data.service
3. then run:
sudo systemctl daemon-reload
sudo systemctl enable --now gps_data.service
sudo systemctl status gps_data.service  # check status
tail -n 100 -f ~/gps_data.log  # watch recorded GPS data

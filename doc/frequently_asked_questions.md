**1. 如何切换挂载目录？**

在start_s3fs.sh中
```bash
LD_LIBRARY_PATH=./libs/:$LD_LIBRARY_PATH nohup ./s3fs -o passwd_file=./conf/passwd -o use_path_request_style -o endpoint=us-east-1 -o url=http://127.0.0.1:9000 -o bucket=test ./mnt -o dbglevel=err -o use_cache=./diskcache -o del_cache -o newcache_conf=./conf/newcache.conf -f >> ./log/s3fs.log 2>&1 &
```
更换其中的 `./mnt` 即可
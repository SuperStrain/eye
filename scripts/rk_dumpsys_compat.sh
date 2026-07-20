#!/bin/sh
# rk_dumpsys_compat.sh — dumpsys 兼容诊断脚本（RV1126B）
#
# 背景：librockit.so（Dec 15, git-5057bd373）与内核 rockit-ko（Dec 30, v2.48.0）协议错配，
#       导致 eye 内置 ipcs_server（TCP 127.0.0.1:3893）对 dumpsys 请求返回 0 字节，
#       /usr/bin/dumpsys 输出空白。
#
# 本脚本绕过 librockit，直接 cat /dev/mpi/* 内核接口，提供等价诊断信息。
#
# 局限：仅暴露 vsys（绑定/节点/帧计数）、valloc（MB）、venc（buf list）、vlog（内核日志）；
#       不提供 dumpsys 的细粒度属性表（VI chn attr、VENC rc_mode 等）。
#
# 用法：
#   rk_dumpsys_compat.sh                # 全量快照
#   rk_dumpsys_compat.sh watch [秒]     # 持续采样计算 fps
#   rk_dumpsys_compat.sh sys|bind|venc|mb|log|version|eye

set -u

MPI_DIR=/dev/mpi
INTERVAL=2

# 安全读取 /dev/mpi/<node>
read_node() {
    [ -e "$MPI_DIR/$1" ] || { echo "(no $MPI_DIR/$1)"; return; }
    cat "$MPI_DIR/$1" 2>/dev/null
}

show_version() {
    echo "========================= Rockit Version ========================="
    echo "[kernel rockit-ko]"
    read_node vsys | grep -E "^version:|^build:" | sed 's/^/  /'
    if [ -r /sys/class/mpi/version ]; then
        echo "  /sys/class/mpi/version: $(cat /sys/class/mpi/version 2>/dev/null)"
    fi
    echo "[userspace librockit.so]"
    if command -v dumpsys >/dev/null 2>&1; then
        dumpsys version 2>/dev/null | grep -E "git-|built-|dumpsys version" | head -3 | sed 's/^/  /'
    fi
    echo "[eye process]"
    pid=$(pidof eye 2>/dev/null | awk '{print $1}')
    if [ -n "$pid" ]; then
        echo "  eye PID=$pid uptime=$(ps -o etime= -p $pid 2>/dev/null | tr -d ' ')"
        echo "  ipcs_server listen: $(netstat -tlnp 2>/dev/null | grep ":3893" | awk '{print $4}')"
    else
        echo "  eye NOT running"
    fi
}

# vsys node list 字段顺序：
# 1=id 2=name 3=handle 4=nid 5=uid 6=ref 7=infa_cnt 8=lcnt 9=frate 10=fbase
# 11=wcnt 12=depth 13=timeout 14=onfa_cnt 15=lcnt 16=frate 17=fbase 18=wcnt
# 19=depth 20=timeout 21=itime 22=otime 23=state 24..=next_node
#
# 帧计数选择：
#   rkvpss-vir0 (VI virtual):  onfa_cnt ($14) 是发往下游的帧数
#   venc (encoder):            infa_cnt ($7) 是从上游接收的帧数
show_bind() {
    echo "========================= Bind Relation & Node State (vsys) ========================="
    vsys_out=$(read_node vsys)
    if [ -z "$vsys_out" ]; then
        echo "ERROR: cannot read $MPI_DIR/vsys"; return
    fi
    # 提取 mpi node list 段并解析
    # 注：内核输出形如 "*****mpi node list*****"，标识符紧贴星号无空格
    # 字段：1=id 2=name 3=handle 4=nid(=VI/VENC 通道号) 5=uid 6=ref
    #       7=infa_cnt 9=in_frate 14=onfa_cnt 16=out_frate 23=state 24..=next_node
    echo "$vsys_out" | awk '
        /mpi node list/ { in_node=1; next }
        /mpi dev list|mpi node frame control/ { in_node=0 }
        in_node && /^[0-9]+/ {
            id=$1; name=$2; nid=$4; uid=$5; ref=$6;
            infc=$7; onfc=$14; frate_in=$9; frate_out=$16;
            state=$23;
            next_nodes="";
            for (i=24; i<=NF; i++) next_nodes = next_nodes" "$i;
            sub(/^ +/, "", next_nodes);
            # 选择有效帧计数：rkvpss 用 onfa_cnt（输出到下游）；venc 用 infa_cnt（从上游收）
            if (name ~ /rkvpss/) { cnt=onfc; dir="out" } else { cnt=infc; dir="in" }
            printf "  node %-3s %-12s chn=%-2s ref=%s frate(in=%s,out=%s) frames_%s=%-10s state=%s -> [%s]\n", \
                id, name, nid, ref, frate_in, frate_out, dir, cnt, state, next_nodes
            nid_arr[++nidx]=id; nname[id]=name; nchn[id]=nid; nnext[id]=next_nodes
        }
        END {
            if (nidx == 0) { print "  (no nodes parsed)"; exit }
            print ""
            print "  bind relations (src -> dst):"
            for (i=1; i<=nidx; i++) {
                id = nid_arr[i]
                if (nnext[id] == "") continue
                nn = split(nnext[id], arr, /[ ]+/)
                for (j=1; j<=nn; j++) {
                    d = arr[j]
                    if (d == "") continue
                    printf "    %s(chn=%s) -> %s(chn=%s)\n", nname[id], nchn[id], nname[d], nchn[d]
                }
            }
        }
    '
}

show_dev() {
    echo "========================= Dev List (vsys) ========================="
    read_node vsys | awk '
        /mpi dev list/ { in_dev=1; next }
        in_dev && /^[0-9]+/ {
            id=$1; name=$2; handle=$3; ref=$4
            rest=""
            for (i=5; i<=NF; i++) rest = rest" "$i
            printf "  dev %-3s %-12s handle=%-10s ref=%s nodes=%s\n", id, name, handle, ref, rest
        }
    '
}

show_venc() {
    echo "========================= VENC buf list ========================="
    read_node venc | awk '
        /^version:/ { print "  rockit-ko "$0; next }
        /buf list/ { in_buf=1; getline; next }
        in_buf && /[0-9]/ { printf "  buf: %s\n", $0 }
    '
}

# valloc 字段：
# buf 行: 1=buf_id 2=(kid) 3=pool 4=ref 5=dma 6=use 7=basefd 8=status 9=size
#         10=handle 11=vaddr 12=paddr 13=memtype 14=create 15=user
# pool 行（在 "mpi pool list" 后）：1=poid 2=nkid 3=total 4=free 5=min
#         6=size 7=reflevel 8=status 9=create 10=user
show_mb() {
    echo "========================= MB Allocation (valloc) ========================="
    read_node valloc | awk '
        /^version:/ { print "  rockit-ko "$0; next }
        /mpi buf list/ { mode="buf"; next }
        /mpi pool list/ { mode="pool"; next }
        /^buf_id/ { next }
        /^poid/ { next }
        mode=="buf" && /^[0-9]+ +\([0-9]+\)/ {
            # $2 形如 (1059)，使用空格分割仍有效
            printf "  buf id=%-3s size=%-10s ref=%s memtype=%-7s create=%-4s user=%s\n", $1, $9, $4, $13, $14, $15
            next
        }
        /^total:/ { printf "  %s\n", $0; next }
        mode=="pool" && /^[0-9]+/ {
            printf "  pool id=%-3s size=%-10s create=%-4s user=%s\n", $1, $6, $9, $10
        }
    '
}

show_log() {
    n=${1:-20}
    echo "========================= Rockit Kernel Log (last $n) ========================="
    read_node vlog | tail -$n | sed 's/^/  /'
}

show_eye_log() {
    n=${1:-20}
    echo "========================= eye Log (last $n) ========================="
    for f in /tmp/eye_warn.log /tmp/eye_info.log; do
        if [ -r "$f" ]; then
            echo "  --- $f (last $n) ---"
            tail -$n "$f" | sed 's/^/  /'
        fi
    done
}

snapshot() {
    show_version; echo ""
    show_bind;    echo ""
    show_dev;     echo ""
    show_venc;    echo ""
    show_mb
}

# 持续采样并计算 fps
watch_fps() {
    interval=${1:-$INTERVAL}
    echo "========================= Frame Rate Watch (interval=${interval}s, Ctrl-C to stop) ========================="
    echo "  帧计数：rkvpss-vir0 用 onfa_cnt，venc 用 infa_cnt"
    echo ""
    # 提取 (id name nid counter) — counter 根据 node 类型选不同字段
    sample() {
        read_node vsys | awk -v t="$(date +%s)" '
            /mpi node list/ { in_node=1; next }
            /mpi dev list|mpi node frame control/ { in_node=0 }
            in_node && /^[0-9]+/ {
                if ($2 ~ /rkvpss/) cnt=$14; else cnt=$7
                print $1, $2, $4, cnt, t
            }
        '
    }
    s1=$(sample)
    printf "%-10s %-32s %-15s %-10s %s\n" "elapsed" "node(chn)=name" "counter" "delta" "fps"
    while true; do
        sleep "$interval"
        s2=$(sample)
        # join by id，计算 delta
        echo "$s2" | while read id2 name2 chn2 cnt2 t2; do
            [ -z "$id2" ] && continue
            cnt1=$(echo "$s1" | awk -v i="$id2" '$1==i {print $4}')
            t1=$(echo "$s1" | awk -v i="$id2" '$1==i {print $5}')
            [ -z "$cnt1" ] && continue
            dt=$((t2 - t1))
            [ "$dt" -le 0 ] && dt=1
            delta=$((cnt2 - cnt1))
            fps=$((delta / dt))
            printf "%-10s node(chn=%s)=%-12s %-15s %-10s %s\n" "${dt}s" "$chn2" "$name2" "$cnt2" "$delta" "$fps"
        done
        s1="$s2"
        echo "---"
    done
}

usage() {
    cat <<EOF
Usage: $0 [module|command]
Modules:
  sys       Bind + dev list + version
  bind      Bind relation and node state (含帧计数)
  vi        Alias of bind
  venc      VENC buf list
  mb        MB allocation (valloc)
  log [N]   Rockit kernel log (default 20)
  eye [N]   eye info/warn log (default 20)
  version   Version info only
Commands:
  watch [s] Continuous fps sampling (interval s, default 2)
  (none)    Full snapshot
EOF
}

case "${1:-all}" in
    sys)     show_version; echo ""; show_bind; echo ""; show_dev ;;
    bind|vi) show_bind ;;
    venc)    show_venc ;;
    mb)      show_mb ;;
    log)     show_log "${2:-20}" ;;
    eye)     show_eye_log "${2:-20}" ;;
    version) show_version ;;
    watch)   watch_fps "${2:-$INTERVAL}" ;;
    all|"")  snapshot ;;
    -h|--help|help) usage ;;
    *)       echo "ERROR: unknown argument: $1" >&2; usage; exit 1 ;;
esac

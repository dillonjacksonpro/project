#!/bin/bash

set -euo pipefail

usage() {
    cat <<'EOF'
Usage: show_partition_limits.sh -p PARTITION [-u USER] [-a ACCOUNT]

Print SLURM service limits for a specific partition.

Options:
  -p, --partition PARTITION   Partition name to inspect
  -u, --user USER             User to query (default: current user)
  -a, --account ACCOUNT       Optional SLURM account to filter associations
  -h, --help                  Show this help message
EOF
}

partition=""
user_name="${USER:-}"
account_name=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        -p|--partition)
            partition="${2:-}"
            shift 2
            ;;
        -u|--user)
            user_name="${2:-}"
            shift 2
            ;;
        -a|--account)
            account_name="${2:-}"
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Error: unknown option: $1" >&2
            usage >&2
            exit 1
            ;;
    esac
done

if [[ -z "$partition" ]]; then
    echo "Error: partition is required" >&2
    usage >&2
    exit 1
fi

echo "=== SLURM service limits ==="
echo "Partition: $partition"
echo "User: $user_name"
if [[ -n "$account_name" ]]; then
    echo "Account: $account_name"
fi
echo

echo "--- Partition record ---"
if ! scontrol show partition "$partition"; then
    echo "Warning: could not read partition details for $partition" >&2
fi
echo

if command -v sacctmgr >/dev/null 2>&1; then
    echo "--- User association limits ---"
    assoc_cmd=(sacctmgr show assoc user="$user_name" format=User,Account,Partition,QOS,GrpJobs,GrpSubmit,MaxJobs,MaxSubmit,MaxTRESPU,MaxTRES,DefQOS -Pn)
    if [[ -n "$account_name" ]]; then
        assoc_cmd+=(account="$account_name")
    fi
    if ! "${assoc_cmd[@]}"; then
        echo "Warning: sacctmgr association query failed or access is restricted" >&2
    fi
    echo
else
    echo "--- User association limits ---"
    echo "sacctmgr is not available on this system"
    echo
fi

echo "--- Relevant QOS records ---"
if command -v sacctmgr >/dev/null 2>&1; then
    qos_list=$(scontrol show partition "$partition" 2>/dev/null | sed -n 's/.*AllowQos=\([^ ]*\).*/\1/p' | tr ',' ' ' || true)
    if [[ -n "$qos_list" ]]; then
        for qos_name in $qos_list; do
            echo "QOS: $qos_name"
            if ! sacctmgr show qos name="$qos_name" format=Name,Priority,MaxJobsPerUser,MaxSubmitJobsPerUser,MaxWall,MaxTRESPU,MaxTRESPJ,GrpJobs,GrpSubmit,GrpTRES -Pn; then
                echo "Warning: could not read QOS $qos_name" >&2
            fi
            echo
        done
    else
        echo "No AllowQos entry found on partition $partition"
    fi
else
    echo "sacctmgr is not available on this system"
fi
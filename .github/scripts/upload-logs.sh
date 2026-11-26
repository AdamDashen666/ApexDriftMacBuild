#!/bin/bash
# 把 $OUT/logs 打包传到私有仓库 release mac-build-<编号>（没有就建）：$1 是文件名前缀，其余参数是一起上传的文件
# 构建机失联时作业日志会整个丢掉，所以每个大阶段一结束就传一次
NAME=$1; shift
TAG="mac-build-${GITHUB_RUN_NUMBER}"
tar -czf "$RUNNER_TEMP/${NAME}-${GITHUB_RUN_NUMBER}.tgz" -C "$OUT" logs
gh release view "$TAG" -R "$PRIVATE_REPO" >/dev/null 2>&1 || gh release create "$TAG" -R "$PRIVATE_REPO" --prerelease --title "Mac 版 #${GITHUB_RUN_NUMBER}" \
  --notes "UE ${UE_REF} 源码编译，Apple 芯片，临时签名（未经苹果公证）。构建：${GITHUB_SERVER_URL}/${GITHUB_REPOSITORY}/actions/runs/${GITHUB_RUN_ID}"
gh release upload "$TAG" -R "$PRIVATE_REPO" --clobber "$RUNNER_TEMP/${NAME}-${GITHUB_RUN_NUMBER}.tgz" "$@"

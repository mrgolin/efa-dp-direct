// SPDX-License-Identifier: Apache-2.0
// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.

#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <cuda_runtime.h>

#include "efa_cuda_dp.h"
#include "efa_cuda_dp_types.h"

static bool is_buf_cleared(void *buf, size_t len)
{
	size_t i;

	for (i = 0; i < len; i++) {
		if (((uint8_t *)buf)[i])
			return false;
	}

	return true;
}

#define is_ext_cleared(ptr, inlen) \
	is_buf_cleared((uint8_t *)ptr + sizeof(*ptr), inlen - sizeof(*ptr))

int efa_cuda_init_cq(struct efa_cuda_cq *cq, struct efa_cuda_cq_attrs *attrs, uint32_t inlen)
{
	if (inlen > sizeof(*attrs) && !is_ext_cleared(attrs, inlen)) {
		printf("Incompatible attributes struct\n");
		return -EINVAL;
	}

	if (__builtin_popcount(attrs->num_entries) != 1) {
		printf("CQ size must be positive power of 2\n");
		return -EINVAL;
	}

	memset(cq, 0, sizeof(*cq));
	cq->buf = attrs->buffer;
	cq->entry_size = attrs->entry_size;
	cq->num_entries = attrs->num_entries;
	cq->queue_mask = attrs->num_entries - 1;
	cq->queue_size_shift = __builtin_ctz(attrs->num_entries);
	cq->phase = 1;

	return 0;
}

int efa_cuda_init_qp(struct efa_cuda_qp *qp, struct efa_cuda_qp_attrs *attrs, uint32_t inlen)
{
	if ((inlen > sizeof(*attrs) && !is_ext_cleared(attrs, inlen)) ||
	    attrs->reserved) {
		printf("Incompatible attributes struct\n");
		return -EINVAL;
	}

	if (__builtin_popcount(attrs->sq_num_entries) != 1 ||
	    __builtin_popcount(attrs->rq_num_entries) != 1) {
		printf("SQ and RQ sizes must be positive powers of 2\n");
		return -EINVAL;
	}

	memset(qp, 0, sizeof(*qp));

	qp->sq.wq.buf = attrs->sq_buffer;
	qp->sq.wq.db = attrs->sq_doorbell;
	qp->sq.wq.max_wqes = attrs->sq_num_entries;
	qp->sq.wq.max_batch = attrs->sq_max_batch;
	qp->sq.wq.queue_mask = attrs->sq_num_entries - 1;
	qp->sq.wq.queue_size_shift = __builtin_ctz(attrs->sq_num_entries);
	// TODO: get from args or delete:
	qp->sq.max_inline_data = 32;
	qp->sq.max_rdma_sges = 2;

	qp->rq.wq.buf = attrs->rq_buffer;
	qp->rq.wq.db = attrs->rq_doorbell;
	qp->rq.wq.max_wqes = attrs->rq_num_entries;
	qp->rq.wq.max_batch = attrs->rq_num_entries;
	qp->rq.wq.queue_mask = attrs->rq_num_entries - 1;
	qp->rq.wq.queue_size_shift = __builtin_ctz(attrs->rq_num_entries);
	qp->rq.wq.phase = 1;

	return 0;
}

struct efa_cuda_cq *efa_cuda_create_cq(struct efa_cuda_cq_attrs *attrs, uint32_t inlen)
{
	cudaError_t cuda_err;
	efa_cuda_cq *d_cq;
	efa_cuda_cq h_cq;
	int ret;

	ret = efa_cuda_init_cq(&h_cq, attrs, inlen);
	if (ret)
		return nullptr;

	cuda_err = cudaMalloc(&d_cq, sizeof(efa_cuda_cq));
	if (cuda_err != cudaSuccess) {
		printf("Failed to allocate device memory for cq: %s\n",
		       cudaGetErrorString(cuda_err));
		return nullptr;
	}

	cuda_err = cudaMemcpy(d_cq, &h_cq, sizeof(efa_cuda_cq), cudaMemcpyHostToDevice);
	if (cuda_err != cudaSuccess) {
		cudaFree(d_cq);
		printf("Failed to copy cq to device: %s\n",
		       cudaGetErrorString(cuda_err));
		return nullptr;
	}

	return d_cq;
}

void efa_cuda_destroy_cq(efa_cuda_cq *d_cq)
{
	cudaFree(d_cq);
}

struct efa_cuda_qp *efa_cuda_create_qp(struct efa_cuda_qp_attrs *attrs, uint32_t inlen)
{
	cudaError_t cuda_err;
	efa_cuda_qp *d_qp;
	efa_cuda_qp h_qp;
	int ret;

	ret = efa_cuda_init_qp(&h_qp, attrs, inlen);
	if (ret)
		return nullptr;

	cuda_err = cudaMalloc(&d_qp, sizeof(efa_cuda_qp));
	if (cuda_err != cudaSuccess) {
		printf("Failed to allocate device memory for qp: %s\n",
		       cudaGetErrorString(cuda_err));
		return nullptr;
	}

	cuda_err = cudaMemcpy(d_qp, &h_qp, sizeof(efa_cuda_qp), cudaMemcpyHostToDevice);
	if (cuda_err != cudaSuccess) {
		cudaFree(d_qp);
		printf("Failed to copy qp to device: %s\n",
		       cudaGetErrorString(cuda_err));
		return nullptr;
	}

	return d_qp;
}

void efa_cuda_destroy_qp(struct efa_cuda_qp *d_qp)
{
	if (d_qp)
		cudaFree(d_qp);
}

int efa_cuda_get_version(int *major, int *minor, int *subminor)
{
	if (!major || !minor || !subminor)
		return -EINVAL;

	*major = EFA_CUDA_DP_VERSION_MAJOR;
	*minor = EFA_CUDA_DP_VERSION_MINOR;
	*subminor = EFA_CUDA_DP_VERSION_SUBMINOR;

	return 0;
}

// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2023 Alibaba, Inc.
 */

#include <unistd.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <linux/err.h>

#include <linux/filter.h>
#include <bpf/bpf.h>
#include <bpf/libbpf.h>

#include "rich_container.skel.h"
#include "cgroup_helpers.h"

#define RICH_CONTAINER_PROG "./rich_container.o"
#define TEST_CGROUP "/test-bpf-rich-container/"
#define SYSPATH "/sys/devices/system/cpu/online"

static char bpf_log_buf[BPF_LOG_BUF_SIZE];

static int load_empty_prog(enum bpf_prog_type prog_type, enum bpf_attach_type attach_type,
			   bool expected_accept)
{
	LIBBPF_OPTS(bpf_prog_load_opts, opts,
		.expected_attach_type = attach_type,
		.log_buf = bpf_log_buf,
		.log_size = BPF_LOG_BUF_SIZE,
		.log_level = 2,
	);
	int ret;
	struct bpf_insn prog[] = {
		BPF_MOV64_IMM(BPF_REG_0, 0),
		BPF_EXIT_INSN(),
	};
	size_t insns_cnt = sizeof(prog) / sizeof(struct bpf_insn);

	ret = bpf_prog_load(prog_type, NULL, "GPL", prog, insns_cnt, &opts);
	if (expected_accept && ret < 0)
		fprintf(stderr, "%s\n", bpf_log_buf);

	return ret;
}

static int get_cpu_info(char *buf, size_t buflen)
{
	ssize_t len;
	int sysfd;

	sysfd = open(SYSPATH, O_RDONLY);
	if (sysfd < 0) {
		printf("Failed to open "SYSPATH"\n");
		return sysfd;
	}

	len = read(sysfd, buf, buflen);
	close(sysfd);
	if (len <= 0) {
		printf("Failed to read "SYSPATH"\n");
		return len;
	}

	buf[len-1] = '\0';
	return len;
}

int main(int argc, char **argv)
{
	struct rich_container *skel = NULL;
	struct bpf_link *link = NULL;
	int error = EXIT_FAILURE;
	int ret;
	int prog_fd, cgroup_fd;
	char origin_cpus[PATH_MAX], now_cpus[PATH_MAX];

	ret = get_cpu_info(origin_cpus, sizeof(origin_cpus));
	if (ret <= 0)
		return 1;

	/*
	 * For empty prog, we only test REJECT cases here,
	 * since ACCEPT cases have been tested in test_verifier.
	 */
	/* test wrong expected_attach_type */
	ret = load_empty_prog(BPF_PROG_TYPE_CGROUP_RICH_CONTAINER, BPF_CGROUP_INET_INGRESS, false);
	if (!ret || errno != EINVAL) {
		printf("Unexpected load result, ret=%d, errno=%d\n", ret, errno);
		return 1;
	}

	skel = rich_container__open_and_load();
	if (!skel) {
		printf("Failed to open and load object\n");
		return 1;
	}

	cgroup_fd = cgroup_setup_and_join(TEST_CGROUP);
	if (cgroup_fd < 0) {
		printf("Failed to create test cgroup\n");
		goto out;
	}

	/* test wrong attach_type */
	prog_fd = load_empty_prog(BPF_PROG_TYPE_CGROUP_RICH_CONTAINER,
				  BPF_CGROUP_RICH_CONTAINER_CPU, true);
	if (prog_fd < 0) {
		printf("Failed to load empty prog\n");
		goto clean_cg;
	}
	ret = bpf_prog_attach(prog_fd, cgroup_fd, BPF_CGROUP_INET_INGRESS, 0);
	if (!ret || errno != EINVAL) {
		printf("Unexpected attach result, ret=%d, errno=%d\n", ret, errno);
		goto clean_cg;
	}

	ret = bpf_prog_attach(prog_fd, cgroup_fd, BPF_CGROUP_RICH_CONTAINER_MEM, 0);
	if (!ret || errno != EINVAL) {
		printf("Unexpected attach result, ret=%d, errno=%d\n", ret, errno);
		goto clean_cg;
	}

	/* test wrong prog_type */
	prog_fd = load_empty_prog(BPF_PROG_TYPE_KPROBE, 0, true);
	if (prog_fd < 0) {
		printf("Failed to load empty prog\n");
		goto clean_cg;
	}
	ret = bpf_prog_attach(prog_fd, cgroup_fd, BPF_CGROUP_RICH_CONTAINER_CPU, 0);
	if (!ret || errno != EINVAL) {
		printf("Unexpected attach result, ret=%d, errno=%d\n", ret, errno);
		goto clean_cg;
	}

	/* Attach cpu program */
	link = bpf_program__attach_cgroup(skel->progs.bpf_prog1, cgroup_fd);
	if (IS_ERR(link)) {
		printf("Failed to attach cgroup\n");
		goto clean_cg;
	}

	ret = get_cpu_info(now_cpus, sizeof(now_cpus));
	if (ret <= 0)
		goto clean_cg;

	if (strcmp(now_cpus, "")) {
		printf("Test rich_container_cpu failed! Expect empty string but get %s\n",
		       now_cpus);
		goto clean_cg;
	}

	skel->bss->test_mode = 1;
	ret = get_cpu_info(now_cpus, sizeof(now_cpus));
	if (ret <= 0)
		goto clean_cg;

	if (strcmp(now_cpus, "0")) {
		printf("Test rich_container_cpu failed! Expect 0 but get %s\n", now_cpus);
		goto clean_cg;
	}

	skel->bss->test_mode = 2;
	ret = get_cpu_info(now_cpus, sizeof(now_cpus));
	if (ret <= 0)
		goto clean_cg;

	if (strcmp(now_cpus, origin_cpus)) {
		printf("Test rich_container_cpu failed! Expect %s but get %s\n",
		       origin_cpus, now_cpus);
		goto clean_cg;
	}

	error = 0;
	printf("test_rich_container:PASS\n");

clean_cg:
	cleanup_cgroup_environment();

out:
	bpf_link__destroy(link);
	rich_container__destroy(skel);
	return error;
}

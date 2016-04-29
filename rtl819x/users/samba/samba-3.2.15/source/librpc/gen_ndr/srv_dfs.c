/*
 * Unix SMB/CIFS implementation.
 * server auto-generated by pidl. DO NOT MODIFY!
 */

#include "includes.h"
#include "librpc/gen_ndr/srv_dfs.h"

static bool api_dfs_GetManagerVersion(pipes_struct *p)
{
	const struct ndr_interface_call *call;
	struct ndr_pull *pull;
	struct ndr_push *push;
	enum ndr_err_code ndr_err;
	DATA_BLOB blob;
	struct dfs_GetManagerVersion *r;

	call = &ndr_table_netdfs.calls[NDR_DFS_GETMANAGERVERSION];

	r = talloc(talloc_tos(), struct dfs_GetManagerVersion);
	if (r == NULL) {
		return false;
	}

	if (!prs_data_blob(&p->in_data.data, &blob, r)) {
		talloc_free(r);
		return false;
	}

	pull = ndr_pull_init_blob(&blob, r);
	if (pull == NULL) {
		talloc_free(r);
		return false;
	}

	pull->flags |= LIBNDR_FLAG_REF_ALLOC;
	ndr_err = call->ndr_pull(pull, NDR_IN, r);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(r);
		return false;
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(dfs_GetManagerVersion, r);
	}

	ZERO_STRUCT(r->out);
	r->out.version = talloc_zero(r, enum dfs_ManagerVersion);
	if (r->out.version == NULL) {
		talloc_free(r);
		return false;
	}

	_dfs_GetManagerVersion(p, r);

	if (p->rng_fault_state) {
		talloc_free(r);
		/* Return true here, srv_pipe_hnd.c will take care */
		return true;
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(dfs_GetManagerVersion, r);
	}

	push = ndr_push_init_ctx(r);
	if (push == NULL) {
		talloc_free(r);
		return false;
	}

	ndr_err = call->ndr_push(push, NDR_OUT, r);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(r);
		return false;
	}

	blob = ndr_push_blob(push);
	if (!prs_copy_data_in(&p->out_data.rdata, (const char *)blob.data, (uint32_t)blob.length)) {
		talloc_free(r);
		return false;
	}

	talloc_free(r);

	return true;
}

static bool api_dfs_Add(pipes_struct *p)
{
	const struct ndr_interface_call *call;
	struct ndr_pull *pull;
	struct ndr_push *push;
	enum ndr_err_code ndr_err;
	DATA_BLOB blob;
	struct dfs_Add *r;

	call = &ndr_table_netdfs.calls[NDR_DFS_ADD];

	r = talloc(talloc_tos(), struct dfs_Add);
	if (r == NULL) {
		return false;
	}

	if (!prs_data_blob(&p->in_data.data, &blob, r)) {
		talloc_free(r);
		return false;
	}

	pull = ndr_pull_init_blob(&blob, r);
	if (pull == NULL) {
		talloc_free(r);
		return false;
	}

	pull->flags |= LIBNDR_FLAG_REF_ALLOC;
	ndr_err = call->ndr_pull(pull, NDR_IN, r);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(r);
		return false;
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(dfs_Add, r);
	}

	r->out.result = _dfs_Add(p, r);

	if (p->rng_fault_state) {
		talloc_free(r);
		/* Return true here, srv_pipe_hnd.c will take care */
		return true;
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(dfs_Add, r);
	}

	push = ndr_push_init_ctx(r);
	if (push == NULL) {
		talloc_free(r);
		return false;
	}

	ndr_err = call->ndr_push(push, NDR_OUT, r);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(r);
		return false;
	}

	blob = ndr_push_blob(push);
	if (!prs_copy_data_in(&p->out_data.rdata, (const char *)blob.data, (uint32_t)blob.length)) {
		talloc_free(r);
		return false;
	}

	talloc_free(r);

	return true;
}

static bool api_dfs_Remove(pipes_struct *p)
{
	const struct ndr_interface_call *call;
	struct ndr_pull *pull;
	struct ndr_push *push;
	enum ndr_err_code ndr_err;
	DATA_BLOB blob;
	struct dfs_Remove *r;

	call = &ndr_table_netdfs.calls[NDR_DFS_REMOVE];

	r = talloc(talloc_tos(), struct dfs_Remove);
	if (r == NULL) {
		return false;
	}

	if (!prs_data_blob(&p->in_data.data, &blob, r)) {
		talloc_free(r);
		return false;
	}

	pull = ndr_pull_init_blob(&blob, r);
	if (pull == NULL) {
		talloc_free(r);
		return false;
	}

	pull->flags |= LIBNDR_FLAG_REF_ALLOC;
	ndr_err = call->ndr_pull(pull, NDR_IN, r);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(r);
		return false;
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(dfs_Remove, r);
	}

	r->out.result = _dfs_Remove(p, r);

	if (p->rng_fault_state) {
		talloc_free(r);
		/* Return true here, srv_pipe_hnd.c will take care */
		return true;
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(dfs_Remove, r);
	}

	push = ndr_push_init_ctx(r);
	if (push == NULL) {
		talloc_free(r);
		return false;
	}

	ndr_err = call->ndr_push(push, NDR_OUT, r);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(r);
		return false;
	}

	blob = ndr_push_blob(push);
	if (!prs_copy_data_in(&p->out_data.rdata, (const char *)blob.data, (uint32_t)blob.length)) {
		talloc_free(r);
		return false;
	}

	talloc_free(r);

	return true;
}

static bool api_dfs_SetInfo(pipes_struct *p)
{
	const struct ndr_interface_call *call;
	struct ndr_pull *pull;
	struct ndr_push *push;
	enum ndr_err_code ndr_err;
	DATA_BLOB blob;
	struct dfs_SetInfo *r;

	call = &ndr_table_netdfs.calls[NDR_DFS_SETINFO];

	r = talloc(talloc_tos(), struct dfs_SetInfo);
	if (r == NULL) {
		return false;
	}

	if (!prs_data_blob(&p->in_data.data, &blob, r)) {
		talloc_free(r);
		return false;
	}

	pull = ndr_pull_init_blob(&blob, r);
	if (pull == NULL) {
		talloc_free(r);
		return false;
	}

	pull->flags |= LIBNDR_FLAG_REF_ALLOC;
	ndr_err = call->ndr_pull(pull, NDR_IN, r);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(r);
		return false;
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(dfs_SetInfo, r);
	}

	r->out.result = _dfs_SetInfo(p, r);

	if (p->rng_fault_state) {
		talloc_free(r);
		/* Return true here, srv_pipe_hnd.c will take care */
		return true;
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(dfs_SetInfo, r);
	}

	push = ndr_push_init_ctx(r);
	if (push == NULL) {
		talloc_free(r);
		return false;
	}

	ndr_err = call->ndr_push(push, NDR_OUT, r);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(r);
		return false;
	}

	blob = ndr_push_blob(push);
	if (!prs_copy_data_in(&p->out_data.rdata, (const char *)blob.data, (uint32_t)blob.length)) {
		talloc_free(r);
		return false;
	}

	talloc_free(r);

	return true;
}

static bool api_dfs_GetInfo(pipes_struct *p)
{
	const struct ndr_interface_call *call;
	struct ndr_pull *pull;
	struct ndr_push *push;
	enum ndr_err_code ndr_err;
	DATA_BLOB blob;
	struct dfs_GetInfo *r;

	call = &ndr_table_netdfs.calls[NDR_DFS_GETINFO];

	r = talloc(talloc_tos(), struct dfs_GetInfo);
	if (r == NULL) {
		return false;
	}

	if (!prs_data_blob(&p->in_data.data, &blob, r)) {
		talloc_free(r);
		return false;
	}

	pull = ndr_pull_init_blob(&blob, r);
	if (pull == NULL) {
		talloc_free(r);
		return false;
	}

	pull->flags |= LIBNDR_FLAG_REF_ALLOC;
	ndr_err = call->ndr_pull(pull, NDR_IN, r);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(r);
		return false;
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(dfs_GetInfo, r);
	}

	ZERO_STRUCT(r->out);
	r->out.info = talloc_zero(r, union dfs_Info);
	if (r->out.info == NULL) {
		talloc_free(r);
		return false;
	}

	r->out.result = _dfs_GetInfo(p, r);

	if (p->rng_fault_state) {
		talloc_free(r);
		/* Return true here, srv_pipe_hnd.c will take care */
		return true;
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(dfs_GetInfo, r);
	}

	push = ndr_push_init_ctx(r);
	if (push == NULL) {
		talloc_free(r);
		return false;
	}

	ndr_err = call->ndr_push(push, NDR_OUT, r);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(r);
		return false;
	}

	blob = ndr_push_blob(push);
	if (!prs_copy_data_in(&p->out_data.rdata, (const char *)blob.data, (uint32_t)blob.length)) {
		talloc_free(r);
		return false;
	}

	talloc_free(r);

	return true;
}

static bool api_dfs_Enum(pipes_struct *p)
{
	const struct ndr_interface_call *call;
	struct ndr_pull *pull;
	struct ndr_push *push;
	enum ndr_err_code ndr_err;
	DATA_BLOB blob;
	struct dfs_Enum *r;

	call = &ndr_table_netdfs.calls[NDR_DFS_ENUM];

	r = talloc(talloc_tos(), struct dfs_Enum);
	if (r == NULL) {
		return false;
	}

	if (!prs_data_blob(&p->in_data.data, &blob, r)) {
		talloc_free(r);
		return false;
	}

	pull = ndr_pull_init_blob(&blob, r);
	if (pull == NULL) {
		talloc_free(r);
		return false;
	}

	pull->flags |= LIBNDR_FLAG_REF_ALLOC;
	ndr_err = call->ndr_pull(pull, NDR_IN, r);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(r);
		return false;
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(dfs_Enum, r);
	}

	ZERO_STRUCT(r->out);
	r->out.info = r->in.info;
	r->out.total = r->in.total;
	r->out.result = _dfs_Enum(p, r);

	if (p->rng_fault_state) {
		talloc_free(r);
		/* Return true here, srv_pipe_hnd.c will take care */
		return true;
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(dfs_Enum, r);
	}

	push = ndr_push_init_ctx(r);
	if (push == NULL) {
		talloc_free(r);
		return false;
	}

	ndr_err = call->ndr_push(push, NDR_OUT, r);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(r);
		return false;
	}

	blob = ndr_push_blob(push);
	if (!prs_copy_data_in(&p->out_data.rdata, (const char *)blob.data, (uint32_t)blob.length)) {
		talloc_free(r);
		return false;
	}

	talloc_free(r);

	return true;
}

static bool api_dfs_Rename(pipes_struct *p)
{
	const struct ndr_interface_call *call;
	struct ndr_pull *pull;
	struct ndr_push *push;
	enum ndr_err_code ndr_err;
	DATA_BLOB blob;
	struct dfs_Rename *r;

	call = &ndr_table_netdfs.calls[NDR_DFS_RENAME];

	r = talloc(talloc_tos(), struct dfs_Rename);
	if (r == NULL) {
		return false;
	}

	if (!prs_data_blob(&p->in_data.data, &blob, r)) {
		talloc_free(r);
		return false;
	}

	pull = ndr_pull_init_blob(&blob, r);
	if (pull == NULL) {
		talloc_free(r);
		return false;
	}

	pull->flags |= LIBNDR_FLAG_REF_ALLOC;
	ndr_err = call->ndr_pull(pull, NDR_IN, r);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(r);
		return false;
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(dfs_Rename, r);
	}

	r->out.result = _dfs_Rename(p, r);

	if (p->rng_fault_state) {
		talloc_free(r);
		/* Return true here, srv_pipe_hnd.c will take care */
		return true;
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(dfs_Rename, r);
	}

	push = ndr_push_init_ctx(r);
	if (push == NULL) {
		talloc_free(r);
		return false;
	}

	ndr_err = call->ndr_push(push, NDR_OUT, r);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(r);
		return false;
	}

	blob = ndr_push_blob(push);
	if (!prs_copy_data_in(&p->out_data.rdata, (const char *)blob.data, (uint32_t)blob.length)) {
		talloc_free(r);
		return false;
	}

	talloc_free(r);

	return true;
}

static bool api_dfs_Move(pipes_struct *p)
{
	const struct ndr_interface_call *call;
	struct ndr_pull *pull;
	struct ndr_push *push;
	enum ndr_err_code ndr_err;
	DATA_BLOB blob;
	struct dfs_Move *r;

	call = &ndr_table_netdfs.calls[NDR_DFS_MOVE];

	r = talloc(talloc_tos(), struct dfs_Move);
	if (r == NULL) {
		return false;
	}

	if (!prs_data_blob(&p->in_data.data, &blob, r)) {
		talloc_free(r);
		return false;
	}

	pull = ndr_pull_init_blob(&blob, r);
	if (pull == NULL) {
		talloc_free(r);
		return false;
	}

	pull->flags |= LIBNDR_FLAG_REF_ALLOC;
	ndr_err = call->ndr_pull(pull, NDR_IN, r);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(r);
		return false;
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(dfs_Move, r);
	}

	r->out.result = _dfs_Move(p, r);

	if (p->rng_fault_state) {
		talloc_free(r);
		/* Return true here, srv_pipe_hnd.c will take care */
		return true;
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(dfs_Move, r);
	}

	push = ndr_push_init_ctx(r);
	if (push == NULL) {
		talloc_free(r);
		return false;
	}

	ndr_err = call->ndr_push(push, NDR_OUT, r);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(r);
		return false;
	}

	blob = ndr_push_blob(push);
	if (!prs_copy_data_in(&p->out_data.rdata, (const char *)blob.data, (uint32_t)blob.length)) {
		talloc_free(r);
		return false;
	}

	talloc_free(r);

	return true;
}

static bool api_dfs_ManagerGetConfigInfo(pipes_struct *p)
{
	const struct ndr_interface_call *call;
	struct ndr_pull *pull;
	struct ndr_push *push;
	enum ndr_err_code ndr_err;
	DATA_BLOB blob;
	struct dfs_ManagerGetConfigInfo *r;

	call = &ndr_table_netdfs.calls[NDR_DFS_MANAGERGETCONFIGINFO];

	r = talloc(talloc_tos(), struct dfs_ManagerGetConfigInfo);
	if (r == NULL) {
		return false;
	}

	if (!prs_data_blob(&p->in_data.data, &blob, r)) {
		talloc_free(r);
		return false;
	}

	pull = ndr_pull_init_blob(&blob, r);
	if (pull == NULL) {
		talloc_free(r);
		return false;
	}

	pull->flags |= LIBNDR_FLAG_REF_ALLOC;
	ndr_err = call->ndr_pull(pull, NDR_IN, r);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(r);
		return false;
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(dfs_ManagerGetConfigInfo, r);
	}

	r->out.result = _dfs_ManagerGetConfigInfo(p, r);

	if (p->rng_fault_state) {
		talloc_free(r);
		/* Return true here, srv_pipe_hnd.c will take care */
		return true;
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(dfs_ManagerGetConfigInfo, r);
	}

	push = ndr_push_init_ctx(r);
	if (push == NULL) {
		talloc_free(r);
		return false;
	}

	ndr_err = call->ndr_push(push, NDR_OUT, r);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(r);
		return false;
	}

	blob = ndr_push_blob(push);
	if (!prs_copy_data_in(&p->out_data.rdata, (const char *)blob.data, (uint32_t)blob.length)) {
		talloc_free(r);
		return false;
	}

	talloc_free(r);

	return true;
}

static bool api_dfs_ManagerSendSiteInfo(pipes_struct *p)
{
	const struct ndr_interface_call *call;
	struct ndr_pull *pull;
	struct ndr_push *push;
	enum ndr_err_code ndr_err;
	DATA_BLOB blob;
	struct dfs_ManagerSendSiteInfo *r;

	call = &ndr_table_netdfs.calls[NDR_DFS_MANAGERSENDSITEINFO];

	r = talloc(talloc_tos(), struct dfs_ManagerSendSiteInfo);
	if (r == NULL) {
		return false;
	}

	if (!prs_data_blob(&p->in_data.data, &blob, r)) {
		talloc_free(r);
		return false;
	}

	pull = ndr_pull_init_blob(&blob, r);
	if (pull == NULL) {
		talloc_free(r);
		return false;
	}

	pull->flags |= LIBNDR_FLAG_REF_ALLOC;
	ndr_err = call->ndr_pull(pull, NDR_IN, r);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(r);
		return false;
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(dfs_ManagerSendSiteInfo, r);
	}

	r->out.result = _dfs_ManagerSendSiteInfo(p, r);

	if (p->rng_fault_state) {
		talloc_free(r);
		/* Return true here, srv_pipe_hnd.c will take care */
		return true;
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(dfs_ManagerSendSiteInfo, r);
	}

	push = ndr_push_init_ctx(r);
	if (push == NULL) {
		talloc_free(r);
		return false;
	}

	ndr_err = call->ndr_push(push, NDR_OUT, r);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(r);
		return false;
	}

	blob = ndr_push_blob(push);
	if (!prs_copy_data_in(&p->out_data.rdata, (const char *)blob.data, (uint32_t)blob.length)) {
		talloc_free(r);
		return false;
	}

	talloc_free(r);

	return true;
}

static bool api_dfs_AddFtRoot(pipes_struct *p)
{
	const struct ndr_interface_call *call;
	struct ndr_pull *pull;
	struct ndr_push *push;
	enum ndr_err_code ndr_err;
	DATA_BLOB blob;
	struct dfs_AddFtRoot *r;

	call = &ndr_table_netdfs.calls[NDR_DFS_ADDFTROOT];

	r = talloc(talloc_tos(), struct dfs_AddFtRoot);
	if (r == NULL) {
		return false;
	}

	if (!prs_data_blob(&p->in_data.data, &blob, r)) {
		talloc_free(r);
		return false;
	}

	pull = ndr_pull_init_blob(&blob, r);
	if (pull == NULL) {
		talloc_free(r);
		return false;
	}

	pull->flags |= LIBNDR_FLAG_REF_ALLOC;
	ndr_err = call->ndr_pull(pull, NDR_IN, r);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(r);
		return false;
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(dfs_AddFtRoot, r);
	}

	ZERO_STRUCT(r->out);
	r->out.unknown2 = r->in.unknown2;
	r->out.result = _dfs_AddFtRoot(p, r);

	if (p->rng_fault_state) {
		talloc_free(r);
		/* Return true here, srv_pipe_hnd.c will take care */
		return true;
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(dfs_AddFtRoot, r);
	}

	push = ndr_push_init_ctx(r);
	if (push == NULL) {
		talloc_free(r);
		return false;
	}

	ndr_err = call->ndr_push(push, NDR_OUT, r);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(r);
		return false;
	}

	blob = ndr_push_blob(push);
	if (!prs_copy_data_in(&p->out_data.rdata, (const char *)blob.data, (uint32_t)blob.length)) {
		talloc_free(r);
		return false;
	}

	talloc_free(r);

	return true;
}

static bool api_dfs_RemoveFtRoot(pipes_struct *p)
{
	const struct ndr_interface_call *call;
	struct ndr_pull *pull;
	struct ndr_push *push;
	enum ndr_err_code ndr_err;
	DATA_BLOB blob;
	struct dfs_RemoveFtRoot *r;

	call = &ndr_table_netdfs.calls[NDR_DFS_REMOVEFTROOT];

	r = talloc(talloc_tos(), struct dfs_RemoveFtRoot);
	if (r == NULL) {
		return false;
	}

	if (!prs_data_blob(&p->in_data.data, &blob, r)) {
		talloc_free(r);
		return false;
	}

	pull = ndr_pull_init_blob(&blob, r);
	if (pull == NULL) {
		talloc_free(r);
		return false;
	}

	pull->flags |= LIBNDR_FLAG_REF_ALLOC;
	ndr_err = call->ndr_pull(pull, NDR_IN, r);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(r);
		return false;
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(dfs_RemoveFtRoot, r);
	}

	ZERO_STRUCT(r->out);
	r->out.unknown = r->in.unknown;
	r->out.result = _dfs_RemoveFtRoot(p, r);

	if (p->rng_fault_state) {
		talloc_free(r);
		/* Return true here, srv_pipe_hnd.c will take care */
		return true;
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(dfs_RemoveFtRoot, r);
	}

	push = ndr_push_init_ctx(r);
	if (push == NULL) {
		talloc_free(r);
		return false;
	}

	ndr_err = call->ndr_push(push, NDR_OUT, r);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(r);
		return false;
	}

	blob = ndr_push_blob(push);
	if (!prs_copy_data_in(&p->out_data.rdata, (const char *)blob.data, (uint32_t)blob.length)) {
		talloc_free(r);
		return false;
	}

	talloc_free(r);

	return true;
}

static bool api_dfs_AddStdRoot(pipes_struct *p)
{
	const struct ndr_interface_call *call;
	struct ndr_pull *pull;
	struct ndr_push *push;
	enum ndr_err_code ndr_err;
	DATA_BLOB blob;
	struct dfs_AddStdRoot *r;

	call = &ndr_table_netdfs.calls[NDR_DFS_ADDSTDROOT];

	r = talloc(talloc_tos(), struct dfs_AddStdRoot);
	if (r == NULL) {
		return false;
	}

	if (!prs_data_blob(&p->in_data.data, &blob, r)) {
		talloc_free(r);
		return false;
	}

	pull = ndr_pull_init_blob(&blob, r);
	if (pull == NULL) {
		talloc_free(r);
		return false;
	}

	pull->flags |= LIBNDR_FLAG_REF_ALLOC;
	ndr_err = call->ndr_pull(pull, NDR_IN, r);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(r);
		return false;
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(dfs_AddStdRoot, r);
	}

	r->out.result = _dfs_AddStdRoot(p, r);

	if (p->rng_fault_state) {
		talloc_free(r);
		/* Return true here, srv_pipe_hnd.c will take care */
		return true;
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(dfs_AddStdRoot, r);
	}

	push = ndr_push_init_ctx(r);
	if (push == NULL) {
		talloc_free(r);
		return false;
	}

	ndr_err = call->ndr_push(push, NDR_OUT, r);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(r);
		return false;
	}

	blob = ndr_push_blob(push);
	if (!prs_copy_data_in(&p->out_data.rdata, (const char *)blob.data, (uint32_t)blob.length)) {
		talloc_free(r);
		return false;
	}

	talloc_free(r);

	return true;
}

static bool api_dfs_RemoveStdRoot(pipes_struct *p)
{
	const struct ndr_interface_call *call;
	struct ndr_pull *pull;
	struct ndr_push *push;
	enum ndr_err_code ndr_err;
	DATA_BLOB blob;
	struct dfs_RemoveStdRoot *r;

	call = &ndr_table_netdfs.calls[NDR_DFS_REMOVESTDROOT];

	r = talloc(talloc_tos(), struct dfs_RemoveStdRoot);
	if (r == NULL) {
		return false;
	}

	if (!prs_data_blob(&p->in_data.data, &blob, r)) {
		talloc_free(r);
		return false;
	}

	pull = ndr_pull_init_blob(&blob, r);
	if (pull == NULL) {
		talloc_free(r);
		return false;
	}

	pull->flags |= LIBNDR_FLAG_REF_ALLOC;
	ndr_err = call->ndr_pull(pull, NDR_IN, r);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(r);
		return false;
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(dfs_RemoveStdRoot, r);
	}

	r->out.result = _dfs_RemoveStdRoot(p, r);

	if (p->rng_fault_state) {
		talloc_free(r);
		/* Return true here, srv_pipe_hnd.c will take care */
		return true;
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(dfs_RemoveStdRoot, r);
	}

	push = ndr_push_init_ctx(r);
	if (push == NULL) {
		talloc_free(r);
		return false;
	}

	ndr_err = call->ndr_push(push, NDR_OUT, r);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(r);
		return false;
	}

	blob = ndr_push_blob(push);
	if (!prs_copy_data_in(&p->out_data.rdata, (const char *)blob.data, (uint32_t)blob.length)) {
		talloc_free(r);
		return false;
	}

	talloc_free(r);

	return true;
}

static bool api_dfs_ManagerInitialize(pipes_struct *p)
{
	const struct ndr_interface_call *call;
	struct ndr_pull *pull;
	struct ndr_push *push;
	enum ndr_err_code ndr_err;
	DATA_BLOB blob;
	struct dfs_ManagerInitialize *r;

	call = &ndr_table_netdfs.calls[NDR_DFS_MANAGERINITIALIZE];

	r = talloc(talloc_tos(), struct dfs_ManagerInitialize);
	if (r == NULL) {
		return false;
	}

	if (!prs_data_blob(&p->in_data.data, &blob, r)) {
		talloc_free(r);
		return false;
	}

	pull = ndr_pull_init_blob(&blob, r);
	if (pull == NULL) {
		talloc_free(r);
		return false;
	}

	pull->flags |= LIBNDR_FLAG_REF_ALLOC;
	ndr_err = call->ndr_pull(pull, NDR_IN, r);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(r);
		return false;
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(dfs_ManagerInitialize, r);
	}

	r->out.result = _dfs_ManagerInitialize(p, r);

	if (p->rng_fault_state) {
		talloc_free(r);
		/* Return true here, srv_pipe_hnd.c will take care */
		return true;
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(dfs_ManagerInitialize, r);
	}

	push = ndr_push_init_ctx(r);
	if (push == NULL) {
		talloc_free(r);
		return false;
	}

	ndr_err = call->ndr_push(push, NDR_OUT, r);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(r);
		return false;
	}

	blob = ndr_push_blob(push);
	if (!prs_copy_data_in(&p->out_data.rdata, (const char *)blob.data, (uint32_t)blob.length)) {
		talloc_free(r);
		return false;
	}

	talloc_free(r);

	return true;
}

static bool api_dfs_AddStdRootForced(pipes_struct *p)
{
	const struct ndr_interface_call *call;
	struct ndr_pull *pull;
	struct ndr_push *push;
	enum ndr_err_code ndr_err;
	DATA_BLOB blob;
	struct dfs_AddStdRootForced *r;

	call = &ndr_table_netdfs.calls[NDR_DFS_ADDSTDROOTFORCED];

	r = talloc(talloc_tos(), struct dfs_AddStdRootForced);
	if (r == NULL) {
		return false;
	}

	if (!prs_data_blob(&p->in_data.data, &blob, r)) {
		talloc_free(r);
		return false;
	}

	pull = ndr_pull_init_blob(&blob, r);
	if (pull == NULL) {
		talloc_free(r);
		return false;
	}

	pull->flags |= LIBNDR_FLAG_REF_ALLOC;
	ndr_err = call->ndr_pull(pull, NDR_IN, r);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(r);
		return false;
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(dfs_AddStdRootForced, r);
	}

	r->out.result = _dfs_AddStdRootForced(p, r);

	if (p->rng_fault_state) {
		talloc_free(r);
		/* Return true here, srv_pipe_hnd.c will take care */
		return true;
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(dfs_AddStdRootForced, r);
	}

	push = ndr_push_init_ctx(r);
	if (push == NULL) {
		talloc_free(r);
		return false;
	}

	ndr_err = call->ndr_push(push, NDR_OUT, r);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(r);
		return false;
	}

	blob = ndr_push_blob(push);
	if (!prs_copy_data_in(&p->out_data.rdata, (const char *)blob.data, (uint32_t)blob.length)) {
		talloc_free(r);
		return false;
	}

	talloc_free(r);

	return true;
}

static bool api_dfs_GetDcAddress(pipes_struct *p)
{
	const struct ndr_interface_call *call;
	struct ndr_pull *pull;
	struct ndr_push *push;
	enum ndr_err_code ndr_err;
	DATA_BLOB blob;
	struct dfs_GetDcAddress *r;

	call = &ndr_table_netdfs.calls[NDR_DFS_GETDCADDRESS];

	r = talloc(talloc_tos(), struct dfs_GetDcAddress);
	if (r == NULL) {
		return false;
	}

	if (!prs_data_blob(&p->in_data.data, &blob, r)) {
		talloc_free(r);
		return false;
	}

	pull = ndr_pull_init_blob(&blob, r);
	if (pull == NULL) {
		talloc_free(r);
		return false;
	}

	pull->flags |= LIBNDR_FLAG_REF_ALLOC;
	ndr_err = call->ndr_pull(pull, NDR_IN, r);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(r);
		return false;
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(dfs_GetDcAddress, r);
	}

	ZERO_STRUCT(r->out);
	r->out.server_fullname = r->in.server_fullname;
	r->out.is_root = r->in.is_root;
	r->out.ttl = r->in.ttl;
	r->out.result = _dfs_GetDcAddress(p, r);

	if (p->rng_fault_state) {
		talloc_free(r);
		/* Return true here, srv_pipe_hnd.c will take care */
		return true;
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(dfs_GetDcAddress, r);
	}

	push = ndr_push_init_ctx(r);
	if (push == NULL) {
		talloc_free(r);
		return false;
	}

	ndr_err = call->ndr_push(push, NDR_OUT, r);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(r);
		return false;
	}

	blob = ndr_push_blob(push);
	if (!prs_copy_data_in(&p->out_data.rdata, (const char *)blob.data, (uint32_t)blob.length)) {
		talloc_free(r);
		return false;
	}

	talloc_free(r);

	return true;
}

static bool api_dfs_SetDcAddress(pipes_struct *p)
{
	const struct ndr_interface_call *call;
	struct ndr_pull *pull;
	struct ndr_push *push;
	enum ndr_err_code ndr_err;
	DATA_BLOB blob;
	struct dfs_SetDcAddress *r;

	call = &ndr_table_netdfs.calls[NDR_DFS_SETDCADDRESS];

	r = talloc(talloc_tos(), struct dfs_SetDcAddress);
	if (r == NULL) {
		return false;
	}

	if (!prs_data_blob(&p->in_data.data, &blob, r)) {
		talloc_free(r);
		return false;
	}

	pull = ndr_pull_init_blob(&blob, r);
	if (pull == NULL) {
		talloc_free(r);
		return false;
	}

	pull->flags |= LIBNDR_FLAG_REF_ALLOC;
	ndr_err = call->ndr_pull(pull, NDR_IN, r);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(r);
		return false;
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(dfs_SetDcAddress, r);
	}

	r->out.result = _dfs_SetDcAddress(p, r);

	if (p->rng_fault_state) {
		talloc_free(r);
		/* Return true here, srv_pipe_hnd.c will take care */
		return true;
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(dfs_SetDcAddress, r);
	}

	push = ndr_push_init_ctx(r);
	if (push == NULL) {
		talloc_free(r);
		return false;
	}

	ndr_err = call->ndr_push(push, NDR_OUT, r);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(r);
		return false;
	}

	blob = ndr_push_blob(push);
	if (!prs_copy_data_in(&p->out_data.rdata, (const char *)blob.data, (uint32_t)blob.length)) {
		talloc_free(r);
		return false;
	}

	talloc_free(r);

	return true;
}

static bool api_dfs_FlushFtTable(pipes_struct *p)
{
	const struct ndr_interface_call *call;
	struct ndr_pull *pull;
	struct ndr_push *push;
	enum ndr_err_code ndr_err;
	DATA_BLOB blob;
	struct dfs_FlushFtTable *r;

	call = &ndr_table_netdfs.calls[NDR_DFS_FLUSHFTTABLE];

	r = talloc(talloc_tos(), struct dfs_FlushFtTable);
	if (r == NULL) {
		return false;
	}

	if (!prs_data_blob(&p->in_data.data, &blob, r)) {
		talloc_free(r);
		return false;
	}

	pull = ndr_pull_init_blob(&blob, r);
	if (pull == NULL) {
		talloc_free(r);
		return false;
	}

	pull->flags |= LIBNDR_FLAG_REF_ALLOC;
	ndr_err = call->ndr_pull(pull, NDR_IN, r);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(r);
		return false;
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(dfs_FlushFtTable, r);
	}

	r->out.result = _dfs_FlushFtTable(p, r);

	if (p->rng_fault_state) {
		talloc_free(r);
		/* Return true here, srv_pipe_hnd.c will take care */
		return true;
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(dfs_FlushFtTable, r);
	}

	push = ndr_push_init_ctx(r);
	if (push == NULL) {
		talloc_free(r);
		return false;
	}

	ndr_err = call->ndr_push(push, NDR_OUT, r);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(r);
		return false;
	}

	blob = ndr_push_blob(push);
	if (!prs_copy_data_in(&p->out_data.rdata, (const char *)blob.data, (uint32_t)blob.length)) {
		talloc_free(r);
		return false;
	}

	talloc_free(r);

	return true;
}

static bool api_dfs_Add2(pipes_struct *p)
{
	const struct ndr_interface_call *call;
	struct ndr_pull *pull;
	struct ndr_push *push;
	enum ndr_err_code ndr_err;
	DATA_BLOB blob;
	struct dfs_Add2 *r;

	call = &ndr_table_netdfs.calls[NDR_DFS_ADD2];

	r = talloc(talloc_tos(), struct dfs_Add2);
	if (r == NULL) {
		return false;
	}

	if (!prs_data_blob(&p->in_data.data, &blob, r)) {
		talloc_free(r);
		return false;
	}

	pull = ndr_pull_init_blob(&blob, r);
	if (pull == NULL) {
		talloc_free(r);
		return false;
	}

	pull->flags |= LIBNDR_FLAG_REF_ALLOC;
	ndr_err = call->ndr_pull(pull, NDR_IN, r);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(r);
		return false;
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(dfs_Add2, r);
	}

	r->out.result = _dfs_Add2(p, r);

	if (p->rng_fault_state) {
		talloc_free(r);
		/* Return true here, srv_pipe_hnd.c will take care */
		return true;
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(dfs_Add2, r);
	}

	push = ndr_push_init_ctx(r);
	if (push == NULL) {
		talloc_free(r);
		return false;
	}

	ndr_err = call->ndr_push(push, NDR_OUT, r);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(r);
		return false;
	}

	blob = ndr_push_blob(push);
	if (!prs_copy_data_in(&p->out_data.rdata, (const char *)blob.data, (uint32_t)blob.length)) {
		talloc_free(r);
		return false;
	}

	talloc_free(r);

	return true;
}

static bool api_dfs_Remove2(pipes_struct *p)
{
	const struct ndr_interface_call *call;
	struct ndr_pull *pull;
	struct ndr_push *push;
	enum ndr_err_code ndr_err;
	DATA_BLOB blob;
	struct dfs_Remove2 *r;

	call = &ndr_table_netdfs.calls[NDR_DFS_REMOVE2];

	r = talloc(talloc_tos(), struct dfs_Remove2);
	if (r == NULL) {
		return false;
	}

	if (!prs_data_blob(&p->in_data.data, &blob, r)) {
		talloc_free(r);
		return false;
	}

	pull = ndr_pull_init_blob(&blob, r);
	if (pull == NULL) {
		talloc_free(r);
		return false;
	}

	pull->flags |= LIBNDR_FLAG_REF_ALLOC;
	ndr_err = call->ndr_pull(pull, NDR_IN, r);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(r);
		return false;
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(dfs_Remove2, r);
	}

	r->out.result = _dfs_Remove2(p, r);

	if (p->rng_fault_state) {
		talloc_free(r);
		/* Return true here, srv_pipe_hnd.c will take care */
		return true;
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(dfs_Remove2, r);
	}

	push = ndr_push_init_ctx(r);
	if (push == NULL) {
		talloc_free(r);
		return false;
	}

	ndr_err = call->ndr_push(push, NDR_OUT, r);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(r);
		return false;
	}

	blob = ndr_push_blob(push);
	if (!prs_copy_data_in(&p->out_data.rdata, (const char *)blob.data, (uint32_t)blob.length)) {
		talloc_free(r);
		return false;
	}

	talloc_free(r);

	return true;
}

static bool api_dfs_EnumEx(pipes_struct *p)
{
	const struct ndr_interface_call *call;
	struct ndr_pull *pull;
	struct ndr_push *push;
	enum ndr_err_code ndr_err;
	DATA_BLOB blob;
	struct dfs_EnumEx *r;

	call = &ndr_table_netdfs.calls[NDR_DFS_ENUMEX];

	r = talloc(talloc_tos(), struct dfs_EnumEx);
	if (r == NULL) {
		return false;
	}

	if (!prs_data_blob(&p->in_data.data, &blob, r)) {
		talloc_free(r);
		return false;
	}

	pull = ndr_pull_init_blob(&blob, r);
	if (pull == NULL) {
		talloc_free(r);
		return false;
	}

	pull->flags |= LIBNDR_FLAG_REF_ALLOC;
	ndr_err = call->ndr_pull(pull, NDR_IN, r);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(r);
		return false;
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(dfs_EnumEx, r);
	}

	ZERO_STRUCT(r->out);
	r->out.info = r->in.info;
	r->out.total = r->in.total;
	r->out.result = _dfs_EnumEx(p, r);

	if (p->rng_fault_state) {
		talloc_free(r);
		/* Return true here, srv_pipe_hnd.c will take care */
		return true;
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(dfs_EnumEx, r);
	}

	push = ndr_push_init_ctx(r);
	if (push == NULL) {
		talloc_free(r);
		return false;
	}

	ndr_err = call->ndr_push(push, NDR_OUT, r);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(r);
		return false;
	}

	blob = ndr_push_blob(push);
	if (!prs_copy_data_in(&p->out_data.rdata, (const char *)blob.data, (uint32_t)blob.length)) {
		talloc_free(r);
		return false;
	}

	talloc_free(r);

	return true;
}

static bool api_dfs_SetInfo2(pipes_struct *p)
{
	const struct ndr_interface_call *call;
	struct ndr_pull *pull;
	struct ndr_push *push;
	enum ndr_err_code ndr_err;
	DATA_BLOB blob;
	struct dfs_SetInfo2 *r;

	call = &ndr_table_netdfs.calls[NDR_DFS_SETINFO2];

	r = talloc(talloc_tos(), struct dfs_SetInfo2);
	if (r == NULL) {
		return false;
	}

	if (!prs_data_blob(&p->in_data.data, &blob, r)) {
		talloc_free(r);
		return false;
	}

	pull = ndr_pull_init_blob(&blob, r);
	if (pull == NULL) {
		talloc_free(r);
		return false;
	}

	pull->flags |= LIBNDR_FLAG_REF_ALLOC;
	ndr_err = call->ndr_pull(pull, NDR_IN, r);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(r);
		return false;
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_IN_DEBUG(dfs_SetInfo2, r);
	}

	r->out.result = _dfs_SetInfo2(p, r);

	if (p->rng_fault_state) {
		talloc_free(r);
		/* Return true here, srv_pipe_hnd.c will take care */
		return true;
	}

	if (DEBUGLEVEL >= 10) {
		NDR_PRINT_OUT_DEBUG(dfs_SetInfo2, r);
	}

	push = ndr_push_init_ctx(r);
	if (push == NULL) {
		talloc_free(r);
		return false;
	}

	ndr_err = call->ndr_push(push, NDR_OUT, r);
	if (!NDR_ERR_CODE_IS_SUCCESS(ndr_err)) {
		talloc_free(r);
		return false;
	}

	blob = ndr_push_blob(push);
	if (!prs_copy_data_in(&p->out_data.rdata, (const char *)blob.data, (uint32_t)blob.length)) {
		talloc_free(r);
		return false;
	}

	talloc_free(r);

	return true;
}


/* Tables */
static struct api_struct api_netdfs_cmds[] = 
{
	{"DFS_GETMANAGERVERSION", NDR_DFS_GETMANAGERVERSION, api_dfs_GetManagerVersion},
	{"DFS_ADD", NDR_DFS_ADD, api_dfs_Add},
	{"DFS_REMOVE", NDR_DFS_REMOVE, api_dfs_Remove},
	{"DFS_SETINFO", NDR_DFS_SETINFO, api_dfs_SetInfo},
	{"DFS_GETINFO", NDR_DFS_GETINFO, api_dfs_GetInfo},
	{"DFS_ENUM", NDR_DFS_ENUM, api_dfs_Enum},
	{"DFS_RENAME", NDR_DFS_RENAME, api_dfs_Rename},
	{"DFS_MOVE", NDR_DFS_MOVE, api_dfs_Move},
	{"DFS_MANAGERGETCONFIGINFO", NDR_DFS_MANAGERGETCONFIGINFO, api_dfs_ManagerGetConfigInfo},
	{"DFS_MANAGERSENDSITEINFO", NDR_DFS_MANAGERSENDSITEINFO, api_dfs_ManagerSendSiteInfo},
	{"DFS_ADDFTROOT", NDR_DFS_ADDFTROOT, api_dfs_AddFtRoot},
	{"DFS_REMOVEFTROOT", NDR_DFS_REMOVEFTROOT, api_dfs_RemoveFtRoot},
	{"DFS_ADDSTDROOT", NDR_DFS_ADDSTDROOT, api_dfs_AddStdRoot},
	{"DFS_REMOVESTDROOT", NDR_DFS_REMOVESTDROOT, api_dfs_RemoveStdRoot},
	{"DFS_MANAGERINITIALIZE", NDR_DFS_MANAGERINITIALIZE, api_dfs_ManagerInitialize},
	{"DFS_ADDSTDROOTFORCED", NDR_DFS_ADDSTDROOTFORCED, api_dfs_AddStdRootForced},
	{"DFS_GETDCADDRESS", NDR_DFS_GETDCADDRESS, api_dfs_GetDcAddress},
	{"DFS_SETDCADDRESS", NDR_DFS_SETDCADDRESS, api_dfs_SetDcAddress},
	{"DFS_FLUSHFTTABLE", NDR_DFS_FLUSHFTTABLE, api_dfs_FlushFtTable},
	{"DFS_ADD2", NDR_DFS_ADD2, api_dfs_Add2},
	{"DFS_REMOVE2", NDR_DFS_REMOVE2, api_dfs_Remove2},
	{"DFS_ENUMEX", NDR_DFS_ENUMEX, api_dfs_EnumEx},
	{"DFS_SETINFO2", NDR_DFS_SETINFO2, api_dfs_SetInfo2},
};

void netdfs_get_pipe_fns(struct api_struct **fns, int *n_fns)
{
	*fns = api_netdfs_cmds;
	*n_fns = sizeof(api_netdfs_cmds) / sizeof(struct api_struct);
}

NTSTATUS rpc_netdfs_init(void)
{
	return rpc_pipe_register_commands(SMB_RPC_INTERFACE_VERSION, "netdfs", "netdfs", api_netdfs_cmds, sizeof(api_netdfs_cmds) / sizeof(struct api_struct));
}

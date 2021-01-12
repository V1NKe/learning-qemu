#define dma_addr_t uint32_t

struct FunReq{
	uint64_t total_size; // = req_total_size                    // <= 127
	char *list[127];
};

struct FunState{
	dma_addr_t addr;
	uint32_t req_total_size; // size <= 0x1FBFF
	uint32_t FunReq_list_idx;
	dma_addr_t result_addr;  // put_result(fun, 1u);赋值 1或2
	FunReq *req;
	AddressSpace *as;
};

void fun_write(struct FunState *fun,int addr,int val) {
	switch (addr) {
	case 0x0:
		fun->req_total_size = val;
		break;
	case 0x4:
		fun->addr = val;
		break;
	case 0x8:
		fun->result_addr = val;
		break;
	case 0xc:
		fun->FunReq_list_idx = val;
		break;
	case 0x10:
		if (fun->req) {
			handle_data_read(fun, fun->req,fun->FunReq_list_idx);
			break;
		}
		else{
			break;
		}
	case 0x14:
		if (fun->req) {
			break;
		}
		else {
			fun->req = create_req(fun->req_total_size);
			break;	
		}
	case 0x18:
		if (fun->req) {
			delete_req(fun->req);
			fun->req = 0;
			fun->req_total_size = 0;
			break;
		}
		else{
			fun->req = 0;
			fun->req_total_size = 0;
			break;
		}
	}

	return 0;
}



int fun_read(struct FunState *fun, int addr) {
	int ret_data = -1;
	switch (addr) {
	case 0x0:
		ret_data = fun->req_total_size;
		break;
	case 0x4:
		ret_data = fun->addr;
		break;
	case 0x8:
		ret_data = fun->result_addr;
		break;
	case 0xc:
		ret_data = fun->FunReq_list_idx;
		break;
	case 0x10:
		if (fun->req) {
			handle_data_write(fun, fun->req,fun->FunReq_list_idx);
			break;
		}
		else {
			break;
		}
	}

	return ret_data;
}
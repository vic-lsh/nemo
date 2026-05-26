import afu_axi_if_pkg::*;
import nemo_defines::*;

module nemo_pipeline_rmw #(
    parameter TRACKING_MODE = 0
) (
     input                                  clk
    ,input                                  rst

    ,input  t_nemo_datapath_ch              nemo_s_in_datapath
    ,input  t_nemo_datapath_valid           nemo_s_in_datapath_valid
    ,output t_nemo_datapath_ready           nemo_s_out_datapath_ready

    ,input  t_nemo_controlpath_req_ch       nemo_s_in_controlpath_req
    ,input  t_nemo_controlpath_req_valid    nemo_s_in_controlpath_req_valid
    ,output t_nemo_controlpath_req_ready    nemo_s_out_controlpath_req_ready

    ,output t_nemo_controlpath_resp_ch      nemo_m_out_controlpath_resp
    ,output t_nemo_controlpath_resp_valid   nemo_m_out_controlpath_resp_valid
    ,input  t_nemo_controlpath_resp_ready   nemo_m_in_controlpath_resp_ready

    ,input  logic [63:0]                    added_stall
);

t_nemo_datapath_ch      nemo_s_in_datapath_postreg;
t_nemo_datapath_valid   nemo_s_in_datapath_postreg_valid;
t_nemo_datapath_ready   nemo_s_out_datapath_postreg_ready;

t_nemo_controlpath_req_ch       nemo_s_in_controlpath_req_postreg;
t_nemo_controlpath_req_valid    nemo_s_in_controlpath_req_postreg_valid;
t_nemo_controlpath_req_ready    nemo_s_out_controlpath_req_postreg_ready;

nemo_datapath_fifo datapath_fifo_inst (
     .clk(clk)
    ,.rst(rst)
    
    ,.nemo_s_in_datapath        (nemo_s_in_datapath)
    ,.nemo_s_in_datapath_valid  (nemo_s_in_datapath_valid)
    ,.nemo_s_out_datapath_ready (nemo_s_out_datapath_ready)

    ,.nemo_m_out_datapath       (nemo_s_in_datapath_postreg)
    ,.nemo_m_out_datapath_valid (nemo_s_in_datapath_postreg_valid)
    ,.nemo_m_in_datapath_ready  (nemo_s_out_datapath_postreg_ready)
);

nemo_controlpath_req_fifo controlpath_req_fifo_inst (
     .clk(clk)
    ,.rst(rst)

    ,.nemo_s_in_controlpath_req (nemo_s_in_controlpath_req)
    ,.nemo_s_in_controlpath_req_valid   (nemo_s_in_controlpath_req_valid)
    ,.nemo_s_out_controlpath_req_ready  (nemo_s_out_controlpath_req_ready)

    ,.nemo_m_out_controlpath_req    (nemo_s_in_controlpath_req_postreg)
    ,.nemo_m_out_controlpath_req_valid  (nemo_s_in_controlpath_req_postreg_valid)
    ,.nemo_m_in_controlpath_req_ready   (nemo_s_out_controlpath_req_postreg_ready)
);

// Turned the FSM into a Pipeline
// Stage 0 - pull from FIFO, calculate addr
// Stage 1 - read state
// Stage 2 - update state
// Stage 3 - write state, send to ctrlpath

/*
Hazards exist in 3 forms:
S1 addr == S0 addr: no problem, will get handled in next cycle
S2 addr == S1 addr: s2_state_new_next before change = s2_state_new
S2 addr == S0 addr: s1_state <= s2_state_new
      |   |   |   |   |
S0      0   1   0'
S1          0d  1   0'
S2              0m  1
RAM                 0m
*/


logic [63:0] stall_cnt;
logic [63:0] added_stall_last;
always @(posedge clk) begin
    if (rst) begin
        stall_cnt <= 0;
        added_stall_last <= 0;
    end else begin
        added_stall_last <= added_stall;
        if (added_stall_last != added_stall) begin
            stall_cnt <= 0;
        end else if (stall_cnt == added_stall_last) begin
            stall_cnt <= 0;
        end else begin
            stall_cnt <= stall_cnt + 1;
        end
    end
end

// SRAM, moved to nemo_defines.sv
// localparam STATE_SIZE_BITS = 64; // to match CSR
// localparam RAM_BYTES = 64 * 1024; // 64 KiB
// localparam N_STATES = RAM_BYTES*8 / STATE_SIZE_BITS; // 8192 states, one per 2MiB region in 16GiB
// localparam SRAM_LOG_DEPTH = $clog2(N_STATES);
// localparam SRAM_STATE_INTERNAL_W = $clog2(STATE_SIZE_BITS);

logic [N_STATES-1:0][STATE_SIZE_BITS-1:0] sram;


// forward declare for hazards
logic s2_valid;
logic [SRAM_LOG_DEPTH-1:0] s2_sram_addr;
logic [STATE_SIZE_BITS-1:0] s2_state_new;

// Stage 0
// Pull from FIFO
logic s0_valid;
logic s0_is_datapath;
logic [SRAM_LOG_DEPTH-1:0] s0_sram_addr;
logic [SRAM_STATE_INTERNAL_W-1:0] s0_state_internal_addr;
t_nemo_datapath_ch s0_datapath_data;
t_nemo_controlpath_req_ch s0_controlpath_data;
assign nemo_s_out_datapath_postreg_ready = stall_cnt == 0;
assign nemo_s_out_controlpath_req_postreg_ready = stall_cnt == 0;

logic s0_valid_next;
logic s0_is_datapath_next;
logic [SRAM_LOG_DEPTH-1:0] s0_sram_addr_next;
logic [SRAM_STATE_INTERNAL_W-1:0] s0_state_internal_addr_next;

always_comb begin
    s0_valid_next = 0;
    s0_is_datapath_next = 0;
    s0_sram_addr_next = '0;
    s0_state_internal_addr_next = '0;
    if (nemo_s_in_controlpath_req_postreg_valid && nemo_s_out_controlpath_req_postreg_ready) begin
        s0_valid_next = 1;
        s0_is_datapath_next = 0;
        s0_sram_addr_next = nemo_s_in_controlpath_req_postreg.pipeline_local_sram_addr[12:0];
        s0_state_internal_addr_next = 0; // doesn't matter for ctrl path
    end else if (nemo_s_in_datapath_postreg_valid && nemo_s_out_datapath_postreg_ready) begin
        s0_valid_next = 1;
        s0_is_datapath_next = 1;
        if (TRACKING_MODE == 0) begin
            s0_state_internal_addr_next = 0; // unimportant
        end else if (TRACKING_MODE == 1) begin
            s0_state_internal_addr_next = nemo_s_in_datapath_postreg.sram_internal_addr;
        end else if (TRACKING_MODE == 2) begin
            s0_state_internal_addr_next = nemo_s_in_datapath_postreg.sram_internal_addr[SRAM_STATE_INTERNAL_W-1:1]*2;
        end
        s0_sram_addr_next = nemo_s_in_datapath_postreg.pipeline_local_sram_addr[12:0];
    end
end

always @(posedge clk) begin
    if (rst) begin
        s0_valid <= 0;
        s0_is_datapath <= 0;
        s0_sram_addr <= '0;
        s0_state_internal_addr <= '0;
        s0_datapath_data <= '0;
        s0_controlpath_data <= '0;
    end else begin
        s0_valid <= s0_valid_next;
        s0_is_datapath <= s0_is_datapath_next;
        s0_sram_addr <= s0_sram_addr_next;
        s0_state_internal_addr <= s0_state_internal_addr_next;
        s0_datapath_data <= nemo_s_in_datapath_postreg;
        s0_controlpath_data <= nemo_s_in_controlpath_req_postreg;
    end
end

// Stage 1
// read state, basically a passthru
logic s1_valid;
logic s1_is_datapath;
logic [SRAM_LOG_DEPTH-1:0] s1_sram_addr;
logic [SRAM_STATE_INTERNAL_W-1:0] s1_state_internal_addr;
logic [STATE_SIZE_BITS-1:0] s1_state;
t_nemo_datapath_ch s1_datapath_data;
t_nemo_controlpath_req_ch s1_controlpath_data;

always @(posedge clk) begin
    if (rst) begin
        s1_valid <= 0;
        s1_is_datapath <= 0;
        s1_sram_addr <= '0;
        s1_state_internal_addr <= '0;
        s1_state <= '0;
        s1_datapath_data <= '0;
        s1_controlpath_data <= '0;
    end else begin
        s1_valid <= s0_valid;
        s1_is_datapath <= s0_is_datapath;
        s1_sram_addr <= s0_sram_addr;
        s1_state_internal_addr <= s0_state_internal_addr;
        if (s0_sram_addr == s2_sram_addr && s2_valid) begin
            // hazard: forward data
            s1_state <= s2_state_new;
        end else begin
            // normal case
            s1_state <= sram[s0_sram_addr];
        end
        s1_datapath_data <= s0_datapath_data;
        s1_controlpath_data <= s0_controlpath_data;
    end
end

// Stage 2: Update state
//logic s2_valid;
logic s2_is_datapath;
//logic [SRAM_LOG_DEPTH-1:0] s2_sram_addr;
//logic [STATE_SIZE_BITS-1:0] s2_state_new;
logic [STATE_SIZE_BITS-1:0] s2_state_old;
t_nemo_controlpath_req_ch s2_controlpath_data;

logic [STATE_SIZE_BITS-1:0] s2_state_new_next;
logic [1:0] mode_2_slice;

always_comb begin
    s2_state_new_next = s1_state;
    // hazard: forward data
    if (s1_sram_addr == s2_sram_addr && s2_valid) begin
        s2_state_new_next = s2_state_new;
    end
    if (s1_is_datapath) begin
        if (TRACKING_MODE == 0) begin
            s2_state_new_next = s2_state_new_next + 1;
        end else if (TRACKING_MODE == 1) begin
            s2_state_new_next[s1_state_internal_addr] = 1;//s2_state_new_next[s1_state_internal_addr] + 1;
        end else if (TRACKING_MODE == 2) begin
            mode_2_slice = s2_state_new_next[s1_state_internal_addr +: 2];
            if (mode_2_slice != 3) begin
                s2_state_new_next[s1_state_internal_addr +: 2] = mode_2_slice + 1;
            end
        end
    end else begin
        s2_state_new_next = 0;
    end
end

always @(posedge clk) begin
    if (rst) begin
        s2_valid <= 0;
        s2_is_datapath <= 0;
        s2_sram_addr <= '0;
        s2_state_new <= '0;
        s2_state_old <= '0;
        s2_controlpath_data <= '0;
    end else begin
        s2_valid <= s1_valid;
        s2_is_datapath <= s1_is_datapath;
        s2_sram_addr <= s1_sram_addr;
        s2_state_new <= s2_state_new_next;
        s2_state_old <= s1_state;
        s2_controlpath_data <= s1_controlpath_data;
    end
end

// Stage 3: write back, output to ctrlpath
assign nemo_m_out_controlpath_resp_valid = s2_valid && !s2_is_datapath;
assign nemo_m_out_controlpath_resp.read_data    = s2_state_old;
assign nemo_m_out_controlpath_resp.req_metadata = s2_controlpath_data.req_metadata;
assign nemo_m_out_controlpath_resp.resp_ok_n[0] = |s2_controlpath_data.addr[20:17]; // bit 16 is the intra-pipeline selector
assign nemo_m_out_controlpath_resp.resp_ok_n[1] = 0;
assign nemo_m_out_controlpath_resp.resp_ok_n[2] = 0;

always @(posedge clk) begin
    if (rst) begin
    end else begin
        if (s2_valid) begin
            sram[s2_sram_addr] <= s2_state_new;
        end
    end
end

endmodule

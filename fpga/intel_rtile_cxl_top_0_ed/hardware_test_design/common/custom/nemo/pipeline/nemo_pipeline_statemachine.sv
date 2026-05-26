import afu_axi_if_pkg::*;
import nemo_defines::*;

module nemo_pipeline_statemachine #(
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

typedef enum logic[7:0] {
    IDLE = 0,
    READ_STATE,
    UPDATE_STATE,
    WRITE_STATE,
    OUTPUT_STATE
} state_t;

state_t state_reg = IDLE;
state_t state_next;

logic working_on_ctrl;

localparam STATE_SIZE = 64; // to match CSR
localparam RAM_BYTES = 1024 * 1024 / 2; // .5 MiB
localparam N_STATES = RAM_BYTES / STATE_SIZE;

logic [N_STATES-1:0][STATE_SIZE-1:0] ram;

logic [$clog2(N_STATES)-1:0] state_readwrite_addr;

logic [STATE_SIZE-1:0] my_state_reg;
logic [STATE_SIZE-1:0] my_state_reg_at_read;
logic [STATE_SIZE-1:0] my_state_reg_at_read_next;
logic [STATE_SIZE-1:0] my_state_next;

logic [63:0] stall_cnt;

always_ff @(posedge clk) begin
    if (rst) begin
        state_reg <= IDLE;
        working_on_ctrl <= 0;
        my_state_reg <= 0;
        stall_cnt <= 0;
        // for (integer i = 0; i < N_STATES; i++) ram[i] <= '0;
        // ram <= '0;
    end else begin
        state_reg <= state_next;
        my_state_reg <= my_state_next;
        my_state_reg_at_read <= my_state_reg_at_read_next;
        if (state_reg == IDLE && state_next == READ_STATE) begin
            if (nemo_s_in_controlpath_req_postreg_valid) begin
                state_readwrite_addr <= nemo_s_in_controlpath_req_postreg.addr[16:3];
                working_on_ctrl <= 1;
            end else begin
                state_readwrite_addr <= nemo_s_in_datapath_postreg.pipeline_local_sram_addr[16:3];
                working_on_ctrl <= 0;
            end
        end else if (state_reg == WRITE_STATE && state_next == OUTPUT_STATE) begin
            ram[state_readwrite_addr] <= my_state_reg;
        end else if (state_reg == OUTPUT_STATE && state_next == IDLE) begin
            stall_cnt <= added_stall;
        end else if (state_reg == IDLE && state_next == IDLE && stall_cnt != 0) begin
            stall_cnt <= stall_cnt - 1;
        end
    end
end

assign nemo_m_out_controlpath_resp.read_data    = my_state_reg_at_read;
assign nemo_m_out_controlpath_resp.req_metadata = nemo_s_in_controlpath_req_postreg.req_metadata;
assign nemo_m_out_controlpath_resp.resp_ok_n[0] = |nemo_s_in_controlpath_req_postreg.addr[20:17];
assign nemo_m_out_controlpath_resp.resp_ok_n[1] = 0;
assign nemo_m_out_controlpath_resp.resp_ok_n[2] = 0;

always_comb begin
    state_next = state_reg;
    my_state_next = my_state_reg;
    my_state_reg_at_read_next = my_state_reg_at_read;
    nemo_s_out_datapath_postreg_ready = 1'b0;
    nemo_s_out_controlpath_req_postreg_ready = 1'b0;
    nemo_m_out_controlpath_resp_valid = 1'b0;

    case (state_reg)
        IDLE: begin
            if (stall_cnt == 0) begin
                if (nemo_s_in_controlpath_req_postreg_valid || nemo_s_in_datapath_postreg_valid) state_next = READ_STATE;
            end
        end
        READ_STATE: begin
            my_state_next = ram[state_readwrite_addr];
            my_state_reg_at_read_next = my_state_next; // aka the same ram read as above
            state_next = UPDATE_STATE;
        end
        UPDATE_STATE: begin
            if (!working_on_ctrl) begin
                if (TRACKING_MODE == 0) begin 
                    // add 1
                    my_state_next = my_state_reg + 1;
                end
            end else begin
                my_state_next = '0;
            end
            state_next = WRITE_STATE;
        end
        WRITE_STATE: begin
            state_next = OUTPUT_STATE;
        end
        OUTPUT_STATE: begin
            if (working_on_ctrl) begin
                nemo_m_out_controlpath_resp_valid = 1;
                if (nemo_m_out_controlpath_resp_valid && nemo_m_in_controlpath_resp_ready) begin
                    nemo_s_out_controlpath_req_postreg_ready = 1;
                    state_next = IDLE;
                end
            end else begin
                nemo_s_out_datapath_postreg_ready = 1;
                state_next = IDLE;
            end
        end
    endcase

end

endmodule

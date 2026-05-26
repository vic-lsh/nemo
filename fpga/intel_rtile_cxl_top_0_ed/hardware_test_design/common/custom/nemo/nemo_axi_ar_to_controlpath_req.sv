import afu_axi_if_pkg::*;
import nemo_defines::*;

module nemo_axi_ar_to_controlpath_reqs #(
     parameter NERRORS = 3
) (
     input                                  clk
    ,input                                  rst

    // Controlpath Inputs
    ,input  t_axi4_rd_addr_ch               controlpath_s_in_axi_ar
    ,output t_axi4_rd_addr_ready            controlpath_s_out_axi_arready

    // Controlpath output
    ,output t_nemo_controlpath_req_ch       nemo_m_out_controlpath_req
    ,output t_nemo_controlpath_req_valid    nemo_m_out_controlpath_req_valid
    ,input  t_nemo_controlpath_req_ready    nemo_m_in_controlpath_req_ready

    ,input  logic   [CHANNEL_W-1:0]         channel

    ,output logic   [NERRORS-1:0]           errors
);
logic deduping;
logic [AFU_AXI_MAX_BURST_LENGTH_WIDTH-1:0]  dedup_amt;

logic [AFU_AXI_MAX_ADDR_WIDTH-1:0]      next_addr;
logic [AFU_AXI_MAX_ID_WIDTH-1:0]        resp_id;

always_comb begin
    nemo_m_out_controlpath_req_valid            = controlpath_s_in_axi_ar.arvalid || deduping;
    nemo_m_out_controlpath_req.addr[AFU_AXI_MAX_ADDR_WIDTH-1:3]       = deduping ? next_addr[AFU_AXI_MAX_ADDR_WIDTH-1:3] : controlpath_s_in_axi_ar.araddr[AFU_AXI_MAX_ADDR_WIDTH-1:3];
    // nemo_m_out_controlpath_req.addr[63:AFU_AXI_MAX_ADDR_WIDTH]       = '0; // turns out AFU_AXI_MAX_ADDR_WIDTH is 64
    nemo_m_out_controlpath_req.req_metadata.cxl_is_source    = 1'b1;
    nemo_m_out_controlpath_req.req_metadata.cxl_src_channel  = channel;
    nemo_m_out_controlpath_req.req_metadata.cxl_resp_id      = deduping ? resp_id   : controlpath_s_in_axi_ar.arid;
    nemo_m_out_controlpath_req.req_metadata.cxl_resp_last    = deduping ? dedup_amt == 1 : 1;
    nemo_m_out_controlpath_req.req_metadata.is_write         = 1'b0;
    nemo_m_out_controlpath_req.write_data       = '0;
    nemo_m_out_controlpath_req.req_metadata.req_ok_n[0]      = 1'b0;
    nemo_m_out_controlpath_req.req_metadata.req_ok_n[1]      = 1'b0;
    nemo_m_out_controlpath_req.req_metadata.req_ok_n[2]      = 1'b0;
    nemo_m_out_controlpath_req.req_metadata.req_ok_n[3]      = 1'b0;
    controlpath_s_out_axi_arready               = nemo_m_in_controlpath_req_ready && !deduping;
end
always @(posedge clk) begin
    if (rst) begin
        deduping <= 0;
        dedup_amt <= 0;
        next_addr <= 0;
    end else begin
        if (nemo_m_out_controlpath_req_valid && nemo_m_in_controlpath_req_ready) begin
            // transaction just occured. do we need to dedup?
            if (deduping) begin
                next_addr <= next_addr + 64;
                dedup_amt <= dedup_amt - 1;
                if (dedup_amt == 1) begin
                        deduping <= 0;
                end
            end else begin
                // not currently deduping
                if (controlpath_s_in_axi_ar.arlen != 0) begin
                        deduping <= 1;
                end
                // outside the if as an optimization. logically inside the above if statement.
                dedup_amt <= controlpath_s_in_axi_ar.arlen;
                next_addr <= controlpath_s_in_axi_ar.araddr + 64; // ASSUMPTION: 64 byte reads only (see error 0)
                resp_id   <= controlpath_s_in_axi_ar.arid;
            end
        end
    end
end

assign errors[0] = controlpath_s_in_axi_ar.arvalid && controlpath_s_in_axi_ar.arsize != esize_512; // we assume every control read arsize is a cacheline
assign errors[1] = controlpath_s_in_axi_ar.arvalid && controlpath_s_in_axi_ar.araddr % 64 != 0;
assign errors[2] = controlpath_s_in_axi_ar.arvalid && controlpath_s_in_axi_ar.arlen != 0 && controlpath_s_in_axi_ar.arburst != eburst_INCR;
endmodule

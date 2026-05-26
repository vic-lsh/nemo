import afu_axi_if_pkg::*;
import nemo_defines::*;
import cxlip_top_pkg::*;

module nemo_controlpath_resp_to_axi #(
     parameter NERRORS = 1
) (
     input                              clk
    ,input                              rst

    // control resp inputs
    ,input  t_nemo_controlpath_resp_ch       nemo_s_in_controlpath_resp
    ,input  t_nemo_controlpath_resp_valid    nemo_s_in_controlpath_resp_valid
    ,output t_nemo_controlpath_resp_ready    nemo_s_out_controlpath_resp_ready

    // control resp outputs
    ,output t_axi4_rd_resp_ch       [MC_CHANNEL-1:0]    controlpath_m_out_axi_r
    ,input  t_axi4_rd_resp_ready    [MC_CHANNEL-1:0]    controlpath_m_in_axi_rready
   
    ,output t_axi4_wr_resp_ch       [MC_CHANNEL-1:0]    controlpath_m_out_axi_b
    ,input  t_axi4_wr_resp_ready    [MC_CHANNEL-1:0]    controlpath_m_in_axi_bready

//     ,output logic   [NERRORS-1:0]            errors
);

assign nemo_s_out_controlpath_resp_ready = controlpath_m_in_axi_rready[nemo_s_in_controlpath_resp.req_metadata.cxl_src_channel];
generate
for (genvar channel = 0; channel < MC_CHANNEL; channel++) begin: per_chan
     always_comb begin
          controlpath_m_out_axi_r[channel].rid        = nemo_s_in_controlpath_resp.req_metadata.cxl_resp_id;
          controlpath_m_out_axi_r[channel].rdata      = {448'b0, nemo_s_in_controlpath_resp.read_data};
          controlpath_m_out_axi_r[channel].rresp      = eresp_OKAY;
          controlpath_m_out_axi_r[channel].rlast      = nemo_s_in_controlpath_resp.req_metadata.cxl_resp_last;
          controlpath_m_out_axi_r[channel].ruser      = '0;

          controlpath_m_out_axi_r[channel].rvalid     = nemo_s_in_controlpath_resp.req_metadata.cxl_src_channel == channel && nemo_s_in_controlpath_resp_valid;


          controlpath_m_out_axi_b[channel]        = '0;
          controlpath_m_out_axi_b[channel].bvalid = 0; // no controlpath writes quite yet.
     end
end
endgenerate
endmodule

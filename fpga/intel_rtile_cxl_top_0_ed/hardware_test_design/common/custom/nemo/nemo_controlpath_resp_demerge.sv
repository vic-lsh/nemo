import nemo_defines::*;

module nemo_controlpath_resp_demerge
(
     input  clk
    ,input  rst

    ,input  t_nemo_controlpath_resp_ch      s_in_controlpath_resp       
    ,input  t_nemo_controlpath_resp_valid   s_in_controlpath_resp_valid 
    ,output t_nemo_controlpath_resp_ready   s_out_controlpath_resp_thenready

    ,output t_nemo_controlpath_resp_ch      m_out_controlpath_cxl_resp        
    ,output t_nemo_controlpath_resp_valid   m_out_controlpath_cxl_resp_valid  
    ,input  t_nemo_controlpath_resp_ready   m_in_controlpath_cxl_resp_ready   
    
    ,output t_nemo_controlpath_resp_ch      m_out_controlpath_mmio_resp        
    ,output t_nemo_controlpath_resp_valid   m_out_controlpath_mmio_resp_valid  
    ,input  t_nemo_controlpath_resp_ready   m_in_controlpath_mmio_resp_ready   
);


assign m_out_controlpath_cxl_resp = s_in_controlpath_resp;
assign m_out_controlpath_mmio_resp = s_in_controlpath_resp;

assign m_out_controlpath_cxl_resp_valid = s_in_controlpath_resp_valid && s_in_controlpath_resp.req_metadata.cxl_is_source;
assign m_out_controlpath_mmio_resp_valid = s_in_controlpath_resp_valid && !s_in_controlpath_resp.req_metadata.cxl_is_source;

always_comb begin
    s_out_controlpath_resp_thenready = 0;
    if (s_in_controlpath_resp_valid) begin
        if (s_in_controlpath_resp.req_metadata.cxl_is_source) begin
            s_out_controlpath_resp_thenready = m_in_controlpath_cxl_resp_ready;
        end else begin
            s_out_controlpath_resp_thenready = m_in_controlpath_mmio_resp_ready;
        end
    end
end

endmodule

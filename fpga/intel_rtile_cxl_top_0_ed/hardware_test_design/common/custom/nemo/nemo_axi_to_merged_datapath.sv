import afu_axi_if_pkg::*;
import nemo_defines::*;
import cxlip_top_pkg::*;

module nemo_axi_to_merged_datapath #(
     parameter  N_DATAPATHS_OUT = 1
    ,localparam PER_DATAPATH_QUEUE_LOG_DEPTH = 7
    ,localparam MERGED_DATAPATH_QUEUE_LOG_DEPTH = 5
    ,localparam DATAPATH_CONVERTER_OUTPUTS = 2
    ,localparam CHANS_PER_DATAPATH = DATAPATH_CONVERTER_OUTPUTS * MC_CHANNEL / N_DATAPATHS_OUT
) (
     input                                                  clk
    ,input                                                  rst

    // Datapath Inputs (we don't get outputs)
    ,input  t_axi4_rd_addr_ch       [MC_CHANNEL-1:0]        datapath_s_in_axi_ar
    ,input  t_axi4_wr_addr_ch       [MC_CHANNEL-1:0]        datapath_s_in_axi_aw
    ,input  t_axi4_wr_data_ch       [MC_CHANNEL-1:0]        datapath_s_in_axi_w

    ,input  t_axi4_rd_addr_ready    [MC_CHANNEL-1:0]        datapath_m_in_axi_arready
    ,input  t_axi4_wr_addr_ready    [MC_CHANNEL-1:0]        datapath_m_in_axi_awready

    ,output logic   [N_DATAPATHS_OUT-1:0][CHANS_PER_DATAPATH-1:0]    datapath_s_out_overflows

    // Datapath output
    ,output t_nemo_datapath_ch      [N_DATAPATHS_OUT-1:0]       nemo_m_out_datapath
    ,output t_nemo_datapath_valid   [N_DATAPATHS_OUT-1:0]       nemo_m_out_datapath_valid
    ,input  t_nemo_datapath_ready   [N_DATAPATHS_OUT-1:0]       nemo_m_in_datapath_ready
);

// convert each req in each channel to a datapath type
t_nemo_datapath_ch      [N_DATAPATHS_OUT-1:0][CHANS_PER_DATAPATH-1:0]    all_datapaths;
t_nemo_datapath_valid   [N_DATAPATHS_OUT-1:0][CHANS_PER_DATAPATH-1:0]    all_datapaths_valid;
t_nemo_datapath_ready   [N_DATAPATHS_OUT-1:0][CHANS_PER_DATAPATH-1:0]    all_datapaths_ready;
generate
if (N_DATAPATHS_OUT != 1 && N_DATAPATHS_OUT != 2) err_me_bc n_datapaths_out_must_be_1_or_2();
for (genvar channel = 0; channel < MC_CHANNEL; channel++) begin: per_chan
    t_nemo_datapath_ch      [DATAPATH_CONVERTER_OUTPUTS-1:0]    datapaths_converted;
    t_nemo_datapath_valid   [DATAPATH_CONVERTER_OUTPUTS-1:0]    datapaths_converted_valid;
    nemo_axi_to_datapath #(
         .N_OUTPUTS(DATAPATH_CONVERTER_OUTPUTS)
    ) datapath_converter (
         .clk(clk)
        ,.rst(rst)

        ,.datapath_s_in_axi_ar(datapath_s_in_axi_ar[channel])
        ,.datapath_s_in_axi_aw(datapath_s_in_axi_aw[channel])
        ,.datapath_s_in_axi_w(datapath_s_in_axi_w[channel])
        ,.datapath_m_in_axi_arready(datapath_m_in_axi_arready[channel])
        ,.datapath_m_in_axi_awready(datapath_m_in_axi_awready[channel])
        ,.nemo_m_out_datapaths(datapaths_converted)
        ,.nemo_m_out_datapaths_valid(datapaths_converted_valid)
        ,.channel(channel[CHANNEL_W-1:0])
    );
    // Throw all the converted datapaths into one array
    for (genvar converter_output = 0; converter_output < DATAPATH_CONVERTER_OUTPUTS; converter_output++) begin: per_converter_output
        /*
        integer ind1;
        integer ind2;
        if (N_DATAPATHS_OUT == 1) begin
            assign ind1 = 0;
            assign ind2 = DATAPATH_CONVERTER_OUTPUTS*channel + converter_output;
        end else begin
            assign ind1 = channel;
            assign ind2 = converter_output;
        end
        assign all_datapaths[ind1][ind2]        = datapaths_converted[converter_output];
        assign all_datapaths_valid[ind1][ind2]  = datapaths_converted_valid[converter_output];
        */
        assign all_datapaths[N_DATAPATHS_OUT == 1 ? 0 : channel][N_DATAPATHS_OUT == 1 ? (DATAPATH_CONVERTER_OUTPUTS*channel + converter_output) : converter_output]        = datapaths_converted[converter_output];
        assign all_datapaths_valid[N_DATAPATHS_OUT == 1 ? 0 : channel][N_DATAPATHS_OUT == 1 ? (DATAPATH_CONVERTER_OUTPUTS*channel + converter_output) : converter_output]  = datapaths_converted_valid[converter_output];
    end
end

for (genvar datapath_out_i = 0; datapath_out_i < N_DATAPATHS_OUT; datapath_out_i++) begin: per_output_datapath
    // convert from CHANS_PER_DATAPATH to 1

    t_nemo_datapath_ch      [CHANS_PER_DATAPATH-1:0]    queued_datapaths;
    t_nemo_datapath_valid   [CHANS_PER_DATAPATH-1:0]    queued_datapaths_valid;
    t_nemo_datapath_ready   [CHANS_PER_DATAPATH-1:0]    queued_datapaths_ready;
    // long queue for each datapath channel
    for (genvar within_datapath_i = 0; within_datapath_i < CHANS_PER_DATAPATH; within_datapath_i++) begin: per_combined_datapath
        logic dp_ready;
        assign datapath_s_out_overflows[datapath_out_i][within_datapath_i] = ~dp_ready;
        nemo_datapath_fifo #(
            .LOG_DEPTH (PER_DATAPATH_QUEUE_LOG_DEPTH)
        ) datapath_fifo (
             .clk                       (clk)
            ,.rst                       (rst)

            ,.nemo_s_in_datapath        (all_datapaths[datapath_out_i][within_datapath_i])
            ,.nemo_s_in_datapath_valid  (all_datapaths_valid[datapath_out_i][within_datapath_i])
            ,.nemo_s_out_datapath_ready (dp_ready)

            ,.nemo_m_out_datapath       (queued_datapaths[within_datapath_i])
            ,.nemo_m_out_datapath_valid (queued_datapaths_valid[within_datapath_i])
            ,.nemo_m_in_datapath_ready  (queued_datapaths_ready[within_datapath_i])
        );
    end

    t_nemo_datapath_ch      merged_datapath;
    t_nemo_datapath_valid   merged_datapath_valid;
    t_nemo_datapath_ready   merged_datapath_ready;
    // merge the (2 or) 4 datapath channels into one
    nemo_datapath_merge #(
        .DATAPATHS_IN  (CHANS_PER_DATAPATH)
    ) datapath_merger (
        .clk                       (clk)
        ,.rst                       (rst)

        ,.datapaths_s_in            (queued_datapaths)
        ,.datapaths_s_in_valid      (queued_datapaths_valid)
        ,.datapaths_s_out_thenready (queued_datapaths_ready)
        ,.datapath_m_out            (merged_datapath)
        ,.datapath_m_out_valid      (merged_datapath_valid)
        ,.datapath_m_in_ready       (merged_datapath_ready)
    );

    // long queue for combined datapaths out
    nemo_datapath_fifo #(
        .LOG_DEPTH (MERGED_DATAPATH_QUEUE_LOG_DEPTH)
    ) datapath_fifo (
         .clk                   (clk)
        ,.rst                   (rst)

        ,.nemo_s_in_datapath         (merged_datapath)
        ,.nemo_s_in_datapath_valid   (merged_datapath_valid)
        ,.nemo_s_out_datapath_ready  (merged_datapath_ready)

        ,.nemo_m_out_datapath        (nemo_m_out_datapath[datapath_out_i])
        ,.nemo_m_out_datapath_valid  (nemo_m_out_datapath_valid[datapath_out_i])
        ,.nemo_m_in_datapath_ready   (nemo_m_in_datapath_ready[datapath_out_i])
    );
end
endgenerate

endmodule

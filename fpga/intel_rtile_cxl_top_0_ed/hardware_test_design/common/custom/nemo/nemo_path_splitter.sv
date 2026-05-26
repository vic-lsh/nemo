import afu_axi_if_pkg::*;
import nemo_defines::*;

module nemo_path_splitter #(
     parameter SIM = 0
) (
     input  logic                       clk
    ,input  logic                       rst

    ,input  t_axi4_rd_addr_ch           s_in_axi_ar
    ,output t_axi4_rd_addr_ready        s_out_axi_arready
    ,output t_axi4_rd_resp_ch           s_out_axi_r
    ,input  t_axi4_rd_resp_ready        s_in_axi_rready

    ,input  t_axi4_wr_addr_ch           s_in_axi_aw
    ,output t_axi4_wr_addr_ready        s_out_axi_awready
    ,input  t_axi4_wr_data_ch           s_in_axi_w
    ,output t_axi4_wr_data_ready        s_out_axi_wready
    ,output t_axi4_wr_resp_ch           s_out_axi_b
    ,input  t_axi4_wr_resp_ready        s_in_axi_bready


    ,output t_axi4_rd_addr_ch           datapath_m_out_axi_ar
    ,input  t_axi4_rd_addr_ready        datapath_m_in_axi_arready
    ,input  t_axi4_rd_resp_ch           datapath_m_in_axi_r
    ,output t_axi4_rd_resp_ready        datapath_m_out_axi_rready

    ,output t_axi4_wr_addr_ch           datapath_m_out_axi_aw
    ,input  t_axi4_wr_addr_ready        datapath_m_in_axi_awready
    ,output t_axi4_wr_data_ch           datapath_m_out_axi_w
    ,input  t_axi4_wr_data_ready        datapath_m_in_axi_wready
    ,input  t_axi4_wr_resp_ch           datapath_m_in_axi_b
    ,output t_axi4_wr_resp_ready        datapath_m_out_axi_bready


    ,output t_axi4_rd_addr_ch           controlpath_m_out_axi_ar
    ,input  t_axi4_rd_addr_ready        controlpath_m_in_axi_arready
    ,input  t_axi4_rd_resp_ch           controlpath_m_in_axi_r
    ,output t_axi4_rd_resp_ready        controlpath_m_out_axi_rready

    ,output t_axi4_wr_addr_ch           controlpath_m_out_axi_aw
    ,input  t_axi4_wr_addr_ready        controlpath_m_in_axi_awready
    ,output t_axi4_wr_data_ch           controlpath_m_out_axi_w
    ,input  t_axi4_wr_data_ready        controlpath_m_in_axi_wready
    ,input  t_axi4_wr_resp_ch           controlpath_m_in_axi_b
    ,output t_axi4_wr_resp_ready        controlpath_m_out_axi_bready

    ,input  [AFU_AXI_MAX_ADDR_WIDTH-1:0]controlpath_start_physical_address

    ,output [8:0] errors_out
);
// accepts a memory request, routes it to cxl or telem output.

// 1. forward writes to datapath, tie off writes in controlpath
axi_reg_aw_old aw_reg (
     .clk   (clk)
    ,.rst   (rst)

    ,.s_in_axi_aw   (s_in_axi_aw)
    ,.s_out_axi_awready (s_out_axi_awready)

    ,.m_out_axi_aw  (datapath_m_out_axi_aw)
    ,.m_in_axi_awready  (datapath_m_in_axi_awready)
);
axi_reg_w_old w_reg (
     .clk   (clk)
    ,.rst   (rst)

    ,.s_in_axi_w   (s_in_axi_w)
    ,.s_out_axi_wready (s_out_axi_wready)

    ,.m_out_axi_w  (datapath_m_out_axi_w)
    ,.m_in_axi_wready  (datapath_m_in_axi_wready)
);
// axi_reg_b_old b_reg (
//      .clk   (clk)
//     ,.rst   (rst)

//     ,.m_in_axi_b    (datapath_m_in_axi_b)
//     ,.m_out_axi_bready  (datapath_m_out_axi_bready)

//     ,.s_out_axi_b   (s_out_axi_b)
//     ,.s_in_axi_bready   (s_in_axi_bready)
// );
always_comb begin
    // write channels <-> datapath
    // datapath_m_out_axi_rready = s_in_axi_rready;
    //datapath_m_out_axi_aw = s_in_axi_aw;
    //datapath_m_out_axi_w = s_in_axi_w;
    datapath_m_out_axi_bready = s_in_axi_bready;

    // s_out_axi_r = datapath_m_in_axi_r;
    //s_out_axi_awready = datapath_m_in_axi_awready;
    //s_out_axi_wready = datapath_m_in_axi_wready;
    s_out_axi_b = datapath_m_in_axi_b;

    // tie off control path write channels
    controlpath_m_out_axi_aw = '0; // includes the valid bit
    controlpath_m_out_axi_w = '0;
    controlpath_m_out_axi_bready = 0;
end

// the read channels continue. axi regs:
t_axi4_rd_addr_ch           m_out_axi_ar;
t_axi4_rd_addr_ready        m_in_axi_arready;
axi_reg_ar_old ar_reg (
     .clk   (clk)
    ,.rst   (rst)

    ,.s_in_axi_ar   (s_in_axi_ar)
    ,.s_out_axi_arready (s_out_axi_arready)

    ,.m_out_axi_ar  (m_out_axi_ar)
    ,.m_in_axi_arready  (m_in_axi_arready)
);

t_axi4_rd_resp_ch           m_in_axi_r;
t_axi4_rd_resp_ready        m_out_axi_rready;
axi_reg_r_old r_reg (
     .clk   (clk)
    ,.rst   (rst)

    ,.m_in_axi_r    (m_in_axi_r)
    ,.m_out_axi_rready  (m_out_axi_rready)

    ,.s_out_axi_r   (s_out_axi_r)
    ,.s_in_axi_rready   (s_in_axi_rready)
);

t_axi4_rd_addr_ch           datapath_m_out_axi_ar_tofifo;
t_axi4_rd_addr_ready        datapath_m_in_axi_arready_tofifo;

logic addr_above_telem_start;
assign addr_above_telem_start = m_out_axi_ar.araddr >= controlpath_start_physical_address;
nemo_axi_read_demux #(
    .SIM(SIM)
)
u_ax_rd_demux (
     .clk                   (clk)
    ,.rst                   (rst)

    ,.s_in_axi_ar           (m_out_axi_ar)
    ,.s_out_axi_arthenready (m_in_axi_arready)
    ,.s_out_axi_r           (m_in_axi_r)
    ,.s_in_axi_rready       (m_out_axi_rready)

    ,.m_out_axi_ar          ({ controlpath_m_out_axi_ar, datapath_m_out_axi_ar_tofifo })
    ,.m_in_axi_arready      ({ controlpath_m_in_axi_arready, datapath_m_in_axi_arready_tofifo })
    ,.m_in_axi_r            ({ controlpath_m_in_axi_r, datapath_m_in_axi_r })
    ,.m_out_axi_rready      ({ controlpath_m_out_axi_rready, datapath_m_out_axi_rready })

    ,.slv_ar_select_i       (addr_above_telem_start)

    ,.errors_out(errors_out)
);

axi_reg_ar_old datapath_ar_reg (
     .clk(clk)
    ,.rst(rst)
    
    ,.s_in_axi_ar   (datapath_m_out_axi_ar_tofifo)
    ,.s_out_axi_arready (datapath_m_in_axi_arready_tofifo)

    ,.m_out_axi_ar  (datapath_m_out_axi_ar)
    ,.m_in_axi_arready  (datapath_m_in_axi_arready)
);

// assign errors_out = '0;

// axi_reg_ar_old ar_reg2 (
//      .clk   (clk)
//     ,.rst   (rst)

//     ,.s_in_axi_ar   (m_out_axi_ar)
//     ,.s_out_axi_arready (m_in_axi_arready)

//     ,.m_out_axi_ar  (datapath_m_out_axi_ar)
//     ,.m_in_axi_arready  (datapath_m_in_axi_arready)
// );

always_comb begin
    // datapath_m_out_axi_ar = m_out_axi_ar;
    // m_in_axi_arready = datapath_m_in_axi_arready;

    // m_in_axi_r = datapath_m_in_axi_r;
    // datapath_m_out_axi_rready = m_out_axi_rready;

    // controlpath_m_out_axi_ar = '0;
    // controlpath_m_out_axi_rready = 0;
end

endmodule

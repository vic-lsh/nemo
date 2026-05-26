import afu_axi_if_pkg::*;
import mc_axi_if_pkg::*;

module nemo_bugfix_rready (
     input clk
    ,input rst

    ,input  t_axi4_rd_addr_ch           s_in_axi_ar
    ,output t_axi4_rd_addr_ready        s_out_axi_arready
    ,output t_axi4_rd_resp_ch           s_out_axi_r
    ,input  t_axi4_rd_resp_ready        s_in_axi_rready

    ,output t_axi4_rd_addr_ch           m_out_axi_ar
    ,input  t_axi4_rd_addr_ready        m_in_axi_arready
    ,input  t_axi4_rd_resp_ch           m_in_axi_r
    ,output t_axi4_rd_resp_ready        m_out_axi_rready
);
// bug: mc doesn't respect rready signal
// soln: we count ar requests and just issue enough that we can always be rready

localparam R_BUFFER_LOG_DEPTH = 6; // enough space to cover mem read latency (~30 cycles)
localparam R_BUFFER_DEPTH = 1 << R_BUFFER_LOG_DEPTH;

logic [R_BUFFER_LOG_DEPTH:0] credits;
logic credit_consume;
logic credit_receive;
logic ar_allowed;

axi_reg_r_old #(
    .LOG_DEPTH(R_BUFFER_LOG_DEPTH)
) r_reg_buffer (
     .clk   (clk)
    ,.rst   (rst)

    ,.m_in_axi_r    (m_in_axi_r)
    ,.m_out_axi_rready  (m_out_axi_rready)

    ,.s_out_axi_r   (s_out_axi_r)
    ,.s_in_axi_rready   (s_in_axi_rready)
);

assign credit_consume = m_out_axi_ar.arvalid && m_in_axi_arready; // whenever memory request is issued to MC
assign credit_receive = s_out_axi_r.rvalid && s_in_axi_rready; // whenever memory response is consumed by AFU
assign ar_allowed = credits != 0;

always_comb begin
    m_out_axi_ar = s_in_axi_ar;
    s_out_axi_arready = m_in_axi_arready;
    if (!ar_allowed) begin
        m_out_axi_ar.arvalid = 0;
        s_out_axi_arready = 0;
    end
end

always @(posedge clk) begin
    if (rst) begin
        credits <= R_BUFFER_DEPTH - 1; // -1 to be safe. fifo should NEVER overflow.
    end else begin
        if (credit_consume && !credit_receive) begin
            credits <= credits - 1;
        end else if (!credit_consume && credit_receive) begin
            credits <= credits + 1;
        end
        if (!m_out_axi_rready) begin
            $fatal("r filled up");
        end
    end
end

endmodule

module nemo_bugfix_req (
    input clk,
    input rst,

    // slave port
    input  t_to_mc_axi4          iafu2mc_to_mc_axi4_in_bugfix_me,
    output t_from_mc_axi4        mc2iafu_from_mc_axi4_out_bugfix_me,

    // master port
    output t_to_mc_axi4          iafu2mc_to_mc_axi4_out,
    input  t_from_mc_axi4        mc2iafu_from_mc_axi4_in
);

// bugfix: can't allow both a read and write transaction on the same cycle, or the MC will drop the read transaction.
// bugfix 2: can't just prioritize write path, because MC only will ever have awready && arready, so a lot of writes
// will starve reads

logic prio;

always @(posedge clk) begin
    if (rst) begin
        prio <= 0;
    end else begin
        prio <= ~prio;
    end
end

always_comb begin
    iafu2mc_to_mc_axi4_out = iafu2mc_to_mc_axi4_in_bugfix_me;
    mc2iafu_from_mc_axi4_out_bugfix_me = mc2iafu_from_mc_axi4_in;

    if (mc2iafu_from_mc_axi4_in.arready && iafu2mc_to_mc_axi4_in_bugfix_me.arvalid && mc2iafu_from_mc_axi4_in.awready && iafu2mc_to_mc_axi4_in_bugfix_me.awvalid) begin
        if (prio) begin
            iafu2mc_to_mc_axi4_out.arvalid = 0;
            mc2iafu_from_mc_axi4_out_bugfix_me.arready = 0;
        end else begin
            // in this simplified model, aw and w are always =
            iafu2mc_to_mc_axi4_out.awvalid = 0;
            mc2iafu_from_mc_axi4_out_bugfix_me.awready = 0;
            iafu2mc_to_mc_axi4_out.wvalid = 0;
            mc2iafu_from_mc_axi4_out_bugfix_me.wready = 0;
        end
    end
    // if (mc2iafu_from_mc_axi4_in.bvalid && iafu2mc_to_mc_axi4_in_bugfix_me.bready) begin
    //     mc2iafu_from_mc_axi4_out_bugfix_me.rvalid = 0;
    //     iafu2mc_to_mc_axi4_out.rready = 0;
    // end
end

endmodule

module nemo_bugfix_resp (
    // input clk,
    // input rst,

    // slave port
    input  t_to_mc_axi4          cxlip2iafu_to_mc_axi4_in,
    output t_from_mc_axi4        iafu2cxlip_from_mc_axi4_out,

    // master port
    output t_to_mc_axi4          cxlip2iafu_to_mc_axi4_out_bugfix_me,
    input  t_from_mc_axi4        iafu2cxlip_from_mc_axi4_in_bugfix_me
);

// bugfix: the MC never issues arready && !awready or vice versa. arready == awready == wready. make it so

always_comb begin
    iafu2cxlip_from_mc_axi4_out = iafu2cxlip_from_mc_axi4_in_bugfix_me;
    cxlip2iafu_to_mc_axi4_out_bugfix_me = cxlip2iafu_to_mc_axi4_in;

    if (iafu2cxlip_from_mc_axi4_in_bugfix_me.arready != iafu2cxlip_from_mc_axi4_in_bugfix_me.awready) begin
        // technically against AXI spec, using ready to influence same channel valid
        // should be fine with FIFOs after
        iafu2cxlip_from_mc_axi4_out.arready = 0;
        iafu2cxlip_from_mc_axi4_out.awready = 0;
        iafu2cxlip_from_mc_axi4_out.wready  = 0;

        cxlip2iafu_to_mc_axi4_out_bugfix_me.arvalid = 0;
        cxlip2iafu_to_mc_axi4_out_bugfix_me.awvalid = 0;
        cxlip2iafu_to_mc_axi4_out_bugfix_me.wvalid  = 0;
    end
end

endmodule

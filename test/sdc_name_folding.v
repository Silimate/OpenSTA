// Hierarchical netlist named the way an RTL elaborator names generate
// scopes (gen[0].child) and 1-bit registers (reg[0]). The SDC names the
// same objects the way a synthesis tool flattens them (gen_0_child, reg).
module sdc_name_folding (clk, in1, status, out1, out2);
  input clk;
  input in1;
  output [3:0] status;
  output out1;
  output out2;
  wire gclk;
  wire q1;

  clkgen clkgen_u0 (.clk(clk), .gclk(gclk));
  ctrl ctrl_u0 (.clk(gclk), .d(in1), .q(q1));
  csr csr_u0 (.clk(clk), .d(q1), .q(status));
  // Two instances whose names fold to the same string (amb_0_x).
  BUFX \amb[0].x  (.A(q1), .Z(out1));
  BUFX amb_0_x (.A(q1), .Z(out2));
endmodule

module clkgen (clk, gclk);
  input clk;
  output gclk;

  cg_wrap \genblk1[0].gen_ch[0].cg_wrap_u0  (.clk(clk), .gclk(gclk));
endmodule

module cg_wrap (clk, gclk);
  input clk;
  output gclk;

  CKGATE icg_u0 (.CP(clk), .E(clk), .Q(gclk));
endmodule

module ctrl (clk, d, q);
  input clk;
  input d;
  output q;

  sync \gen_lane[0].sync_u  (.clk(clk), .d(d), .q(q));
endmodule

module sync (clk, d, q);
  input clk;
  input d;
  output q;
  wire n1;
  wire n2;
  wire n3;

  DFFCLK \ready_meta_0_reg[0]  (.CLK(clk), .D(d), .Q(n1));
  DFFCLK ready_sync_reg (.CLK(clk), .D(n1), .Q(n2));
  DFFCLK \two_bit_reg[0]  (.CLK(clk), .D(n2), .Q(n3));
  DFFCLK \two_bit_reg[1]  (.CLK(clk), .D(n3), .Q(q));
endmodule

module csr (clk, d, q);
  input clk;
  input d;
  output [3:0] q;
  wire ld;
  wire bd;

  DFFCLK \mode_sel_reg[0]  (.CLK(clk), .D(d), .Q(q[0]));
  DFFCLK \mode_sel_reg[1]  (.CLK(clk), .D(d), .Q(q[1]));
  LATEN hold_lat_reg (.EN(clk), .D(d), .Q(ld));
  BUFX buf_u0 (.A(ld), .Z(bd));
  DFF2 dual_reg (.CLKA(clk), .CLKB(clk), .DA(bd), .DB(bd), .QA(q[2]), .QB(q[3]));
endmodule

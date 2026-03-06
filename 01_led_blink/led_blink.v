//----------------------------------------------------------------
// LED Blink Module — PolarFire SoC Discovery Kit
// Clock: 50 MHz fabric clock
// Blink rate: 1 Hz (configurable via parameters)
//----------------------------------------------------------------
module led_blink #(
    parameter CLK_FREQ_HZ   = 50_000_000,
    parameter BLINK_FREQ_HZ = 1
)(
    input  wire clk,
    input  wire rst_n,
    output reg  led
);
    localparam COUNT_MAX = (CLK_FREQ_HZ / (2 * BLINK_FREQ_HZ)) - 1;

    reg [25:0] counter;

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            counter <= 26'd0;
            led     <= 1'b0;
        end else begin
            if (counter == COUNT_MAX) begin
                counter <= 26'd0;
                led     <= ~led;
            end else begin
                counter <= counter + 1'b1;
            end
        end
    end

endmodule
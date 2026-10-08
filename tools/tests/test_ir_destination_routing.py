"""Static routing contract for the IR transmitter's shared radio task."""
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
TX = ROOT / "firmware/programs/IR_SCOPE_ESPNOW/variants/IR_SCOPE_ESPNOW_TX/IR_SCOPE_ESPNOW_TX.ino"
RX = ROOT / "firmware/programs/IR_SCOPE_ESPNOW/variants/IR_SCOPE_ESPNOW_RX/IR_SCOPE_ESPNOW_RX.ino"


class DestinationRoutingTests(unittest.TestCase):
    def test_types_5_and_6_are_unicast_while_legacy_radio_send_is_broadcast(self):
        source = TX.read_text()
        self.assertIn("radioSendTo(BROADCAST,data,len)", source)
        self.assertIn("radioSendTo(OTTO_MAC,data,len)", source)
        self.assertIn("radioSendOtto((uint8_t*)&movement,sizeof(movement))", source)
        self.assertIn("radioSendOtto((uint8_t*)&pulseEvent,sizeof(pulseEvent))", source)
        self.assertIn("radioSend((uint8_t*)&r.packet,sizeof(r.packet))", source)
        self.assertIn("radioSend((uint8_t*)&o,sizeof(o))", source)
        self.assertIn("radioSend((uint8_t*)&p,sizeof(p))", source)

    def test_type_3_type_4_fusion_exchange_is_unchanged(self):
        source = RX.read_text()
        self.assertIn("FUSION_TYPE = 3, ACK_TYPE = 4", source)
        self.assertIn("if(len==86)", source)
        self.assertIn("esp_now_send(request.mac,(uint8_t*)&ack,sizeof(ack))", source)


if __name__ == "__main__":
    unittest.main()

import 'dart:typed_data';

import 'package:flutter_test/flutter_test.dart';
import 'package:hcm/protocol/hcm_protocol.dart';

void main() {
  test('bulk session START round-trips protocol 3.2 layout', () {
    final start = BulkSessionStart(
      host: '192.168.43.1',
      port: 41234,
      token: 'tok_abc',
      afterId: 42,
      mode: recModeFull,
      flags: bulkSessionFlagOtaModel | bulkSessionFlagOtaFirmware,
    );
    final encoded = encodeBulkSessionStart(start);
    expect(encoded[0], bulkSessionCmdStart);
    expect(encoded[1], recModeFull);
    final view = ByteData.sublistView(encoded);
    expect(view.getUint16(2, Endian.little), start.flags);
    expect(view.getUint16(4, Endian.little), 41234);
    expect(view.getUint32(6, Endian.little), 42);
    final decoded = decodeBulkSessionStart(encoded);
    expect(decoded, isNotNull);
    expect(decoded!.host, start.host);
    expect(decoded.port, start.port);
    expect(decoded.token, start.token);
    expect(decoded.afterId, start.afterId);
    expect(decoded.mode, start.mode);
    expect(decoded.flags, start.flags);
  });

  test('bulk session ACK/ABORT opcodes', () {
    final ack = encodeBulkSessionAck(99, mode: recModeFull);
    expect(ack.length, 6);
    expect(ack[0], bulkSessionCmdAck);
    expect(ByteData.sublistView(ack).getUint32(1, Endian.little), 99);
    expect(ack[5], recModeFull);
    final sumAck = encodeBulkSessionAck(7, mode: recModeSummaryOnly);
    expect(sumAck[5], recModeSummaryOnly);
    expect(encodeBulkSessionAbort().single, bulkSessionCmdAbort);
  });

  test('bulk session status is 18-byte packed layout', () {
    final bytes = Uint8List(bulkSessionStatusStructSize);
    final view = ByteData.sublistView(bytes);
    view.setUint8(0, bulkSessionStateTransferring);
    view.setInt8(1, -5);
    view.setUint32(2, 7, Endian.little); // pending
    view.setUint32(6, 11, Endian.little); // cursor
    view.setUint32(10, 11, Endian.little); // ack
    view.setUint32(14, 3, Endian.little); // sent
    final status = decodeBulkSessionStatus(bytes)!;
    expect(status.state, bulkSessionStateTransferring);
    expect(status.error, -5);
    expect(status.pending, 7);
    expect(status.cursorId, 11);
    expect(status.ackId, 11);
    expect(status.sentCount, 3);
    expect(status.upToId, 11);
  });
}

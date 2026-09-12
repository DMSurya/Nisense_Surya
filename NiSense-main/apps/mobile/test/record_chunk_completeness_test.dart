import 'package:flutter_test/flutter_test.dart';
import 'package:hcm/services/record_csv_exporter.dart';

void main() {
  group('RecordCsvExporter.chunksComplete', () {
    test('empty is complete', () {
      expect(RecordCsvExporter.chunksComplete([]), isTrue);
    });

    test('full index set is complete', () {
      final decoded = [
        {
          'header': {'chunk_index': 0, 'chunk_count': 3},
        },
        {
          'header': {'chunk_index': 1, 'chunk_count': 3},
        },
        {
          'header': {'chunk_index': 2, 'chunk_count': 3},
        },
      ];
      expect(RecordCsvExporter.chunksComplete(decoded), isTrue);
    });

    test('missing middle index is incomplete', () {
      final decoded = [
        {
          'header': {'chunk_index': 0, 'chunk_count': 3},
        },
        {
          'header': {'chunk_index': 2, 'chunk_count': 3},
        },
      ];
      expect(RecordCsvExporter.chunksComplete(decoded), isFalse);
    });
  });
}

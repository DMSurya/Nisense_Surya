import 'package:flutter/material.dart';

import '../../theme/nisense_colors.dart';
import '../../theme/nisense_icons.dart';

class GlassCard extends StatelessWidget {
  const GlassCard({super.key, required this.child, this.padding = const EdgeInsets.all(16)});

  final Widget child;
  final EdgeInsets padding;

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: padding,
      decoration: BoxDecoration(
        color: NiSenseColors.bgCard,
        borderRadius: BorderRadius.circular(12),
        border: Border.all(color: NiSenseColors.borderMid),
      ),
      child: child,
    );
  }
}

class MetricBadge extends StatelessWidget {
  const MetricBadge({
    super.key,
    required this.label,
    required this.value,
    required this.unit,
    required this.paramKey,
    this.confidence,
    this.onTap,
  });

  final String label;
  final String value;
  final String unit;
  final String paramKey;
  final int? confidence;
  final VoidCallback? onTap;

  @override
  Widget build(BuildContext context) {
    final color = NiSenseColors.paramColor(paramKey);
    Widget card = GlassCard(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              _paramIcon(paramKey, color),
              const SizedBox(width: 6),
              Expanded(
                child: Text(label, style: Theme.of(context).textTheme.bodySmall),
              ),
            ],
          ),
          const SizedBox(height: 8),
          Row(
            crossAxisAlignment: CrossAxisAlignment.baseline,
            textBaseline: TextBaseline.alphabetic,
            children: [
              Text(
                value,
                style: Theme.of(context).textTheme.titleLarge?.copyWith(color: color),
              ),
              const SizedBox(width: 4),
              Text(unit, style: Theme.of(context).textTheme.bodySmall),
            ],
          ),
          if (confidence != null) ...[
            const SizedBox(height: 4),
            Text('conf $confidence%', style: Theme.of(context).textTheme.bodySmall),
          ],
        ],
      ),
    );
    if (onTap != null) {
      card = GestureDetector(onTap: onTap, child: card);
    }
    return card;
  }

  static Widget _paramIcon(String key, Color color) {
    // bp has no svg — use material icon fallback
    if (key == 'bp') {
      return Icon(Icons.monitor_heart_outlined, size: 18, color: color);
    }
    return NiSenseIcons.paramIcon(key, size: 18, color: color);
  }
}

class ConnectionPill extends StatelessWidget {
  const ConnectionPill({super.key, required this.connected, this.label});

  final bool connected;
  final String? label;

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 6),
      decoration: BoxDecoration(
        color: (connected ? NiSenseColors.statusNormal : NiSenseColors.accentRed)
            .withValues(alpha: 0.15),
        borderRadius: BorderRadius.circular(16),
        border: Border.all(
          color: connected ? NiSenseColors.statusNormal : NiSenseColors.accentRed,
        ),
      ),
      child: Text(
        label ?? (connected ? 'Connected' : 'Disconnected'),
        style: TextStyle(
          color: connected ? NiSenseColors.statusNormal : NiSenseColors.accentRed,
          fontSize: 12,
        ),
      ),
    );
  }
}

/// Press-and-hold button matching device UI (`CONFIG_UI_MEASURE_LONG_PRESS_MS`).
/// Progress fills left→right; releasing early cancels without calling [onHoldComplete].
class HoldToStartButton extends StatefulWidget {
  const HoldToStartButton({
    super.key,
    required this.label,
    required this.onHoldComplete,
    this.enabled = true,
    this.holdDuration = const Duration(milliseconds: 5000),
  });

  final String label;
  final VoidCallback onHoldComplete;
  final bool enabled;
  final Duration holdDuration;

  @override
  State<HoldToStartButton> createState() => _HoldToStartButtonState();
}

class _HoldToStartButtonState extends State<HoldToStartButton>
    with SingleTickerProviderStateMixin {
  late final AnimationController _ctrl;
  bool _holding = false;

  @override
  void initState() {
    super.initState();
    _ctrl = AnimationController(vsync: this, duration: widget.holdDuration)
      ..addStatusListener((status) {
        if (status == AnimationStatus.completed) {
          setState(() => _holding = false);
          widget.onHoldComplete();
          _ctrl.reset();
        }
      });
  }

  @override
  void didUpdateWidget(covariant HoldToStartButton oldWidget) {
    super.didUpdateWidget(oldWidget);
    if (oldWidget.holdDuration != widget.holdDuration) {
      _ctrl.duration = widget.holdDuration;
    }
    if (!widget.enabled && _holding) {
      _cancelHold();
    }
  }

  @override
  void dispose() {
    _ctrl.dispose();
    super.dispose();
  }

  void _cancelHold() {
    _ctrl.stop();
    _ctrl.reset();
    if (_holding) {
      setState(() => _holding = false);
    }
  }

  void _onPointerDown(PointerDownEvent _) {
    if (!widget.enabled) return;
    setState(() => _holding = true);
    _ctrl.forward(from: 0);
  }

  void _onPointerUp(PointerUpEvent _) => _cancelHold();

  void _onPointerCancel(PointerCancelEvent _) => _cancelHold();

  @override
  Widget build(BuildContext context) {
    final scheme = Theme.of(context).colorScheme;
    final bg = widget.enabled ? scheme.primary : scheme.surfaceContainerHighest;
    final fg = widget.enabled ? scheme.onPrimary : scheme.onSurface.withValues(alpha: 0.38);

    return Listener(
      onPointerDown: _onPointerDown,
      onPointerUp: _onPointerUp,
      onPointerCancel: _onPointerCancel,
      child: AnimatedBuilder(
        animation: _ctrl,
        builder: (context, child) {
          return ClipRRect(
            borderRadius: BorderRadius.circular(20),
            child: Stack(
              alignment: Alignment.center,
              children: [
                ColoredBox(
                  color: bg,
                  child: const SizedBox(height: 40, width: double.infinity),
                ),
                if (_holding)
                  Positioned.fill(
                    child: FractionallySizedBox(
                      alignment: Alignment.centerLeft,
                      // Device UI: bar starts full and counts down.
                      widthFactor: (1.0 - _ctrl.value).clamp(0.0, 1.0),
                      child: ColoredBox(
                        color: scheme.onPrimary.withValues(alpha: 0.28),
                      ),
                    ),
                  ),
                Padding(
                  padding: const EdgeInsets.symmetric(horizontal: 20),
                  child: Text(
                    _holding ? 'Hold…' : widget.label,
                    style: TextStyle(
                      color: fg,
                      fontWeight: FontWeight.w600,
                      fontSize: 14,
                    ),
                  ),
                ),
              ],
            ),
          );
        },
      ),
    );
  }
}

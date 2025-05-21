import 'package:flutter/material.dart';
import 'package:google_fonts/google_fonts.dart';

class SetTimeRelays extends StatelessWidget {
  final List<TimeOfDay> onTimes;
  final List<TimeOfDay> offTimes;
  final List<bool> relayStates;
  final Function(BuildContext, int, bool) selectTime;
  final Function(int, bool) toggleRelay;
  final Function() sendSettingsToESP;

  const SetTimeRelays({
    super.key,
    required this.onTimes,
    required this.offTimes,
    required this.relayStates,
    required this.selectTime,
    required this.toggleRelay,
    required this.sendSettingsToESP,
  });

  String _formatTime(TimeOfDay time) {
    return '${time.hour.toString().padLeft(2, '0')}:${time.minute.toString().padLeft(2, '0')}';
  }

  Widget _buildRelayControl(int index, BuildContext context) {
    return Card(
      elevation: 4,
      shadowColor: Colors.purple.withOpacity(0.2),
      shape: RoundedRectangleBorder(
        borderRadius: BorderRadius.circular(12),
      ),
      margin: const EdgeInsets.all(4),
      child: Container(
        decoration: BoxDecoration(
          gradient: LinearGradient(
            colors: [Colors.purple[50]!, Colors.white],
            begin: Alignment.topLeft,
            end: Alignment.bottomRight,
          ),
          borderRadius: BorderRadius.circular(12),
        ),
        child: Padding(
          padding: const EdgeInsets.all(13.0), // Reduced padding
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            mainAxisSize: MainAxisSize.min, // Minimize column height
            children: [
              Text(
                'Relay ${index + 1}',
                style: GoogleFonts.roboto(
                  fontSize: 13.5, // Reduced font size
                  fontWeight: FontWeight.bold,
                  color: Colors.purple[900],
                ),
              ),
              const SizedBox(height: 3), // Reduced spacing
              Row(
                mainAxisAlignment: MainAxisAlignment.spaceBetween,
                children: [
                  Flexible(
                    flex: 1,
                    child: Text(
                      'ON',
                      style: GoogleFonts.roboto(
                        fontSize: 10, // Reduced font size
                        color: Colors.purple[700],
                        fontWeight: FontWeight.w500,
                      ),
                      overflow: TextOverflow.ellipsis,
                    ),
                  ),
                  Flexible(
                    flex: 2,
                    fit: FlexFit.tight,
                    child: OutlinedButton(
                      onPressed: () => selectTime(context, index, true),
                      style: OutlinedButton.styleFrom(
                        foregroundColor: Colors.purple[600],
                        side: BorderSide(color: Colors.purple[600]!),
                        shape: RoundedRectangleBorder(
                          borderRadius: BorderRadius.circular(6),
                        ),
                        backgroundColor: Colors.purple[50],
                        padding: const EdgeInsets.symmetric(
                            horizontal: 6, vertical: 4),
                      ),
                      child: Row(
                        mainAxisSize: MainAxisSize.min,
                        children: [
                          Icon(Icons.access_time,
                              size: 12, color: Colors.purple[600]),
                          const SizedBox(width: 3),
                          Text(
                            _formatTime(onTimes[index]),
                            style: GoogleFonts.roboto(
                              fontSize: 9.5, // Reduced font size
                              color: Colors.purple[800],
                            ),
                          ),
                        ],
                      ),
                    ),
                  ),
                ],
              ),
              const SizedBox(height: 3), // Reduced spacing
              Row(
                mainAxisAlignment: MainAxisAlignment.spaceBetween,
                children: [
                  Flexible(
                    flex: 1,
                    child: Text(
                      'OFF',
                      style: GoogleFonts.roboto(
                        fontSize: 10, // Reduced font size
                        color: Colors.purple[700],
                        fontWeight: FontWeight.w500,
                      ),
                      overflow: TextOverflow.ellipsis,
                    ),
                  ),
                  Flexible(
                    flex: 2,
                    fit: FlexFit.tight,
                    child: OutlinedButton(
                      onPressed: () => selectTime(context, index, false),
                      style: OutlinedButton.styleFrom(
                        foregroundColor: Colors.purple[600],
                        side: BorderSide(color: Colors.purple[600]!),
                        shape: RoundedRectangleBorder(
                          borderRadius: BorderRadius.circular(6),
                        ),
                        backgroundColor: Colors.purple[50],
                        padding: const EdgeInsets.symmetric(
                            horizontal: 6, vertical: 4),
                      ),
                      child: Row(
                        mainAxisSize: MainAxisSize.min,
                        children: [
                          Icon(Icons.access_time,
                              size: 12, color: Colors.purple[600]),
                          const SizedBox(width: 3),
                          Text(
                            _formatTime(offTimes[index]),
                            style: GoogleFonts.roboto(
                              fontSize: 9.5, // Reduced font size
                              color: Colors.purple[800],
                            ),
                          ),
                        ],
                      ),
                    ),
                  ),
                ],
              ),
              const SizedBox(height: 3), // Reduced spacing
              Row(
                mainAxisAlignment: MainAxisAlignment.spaceBetween,
                children: [
                  Flexible(
                    flex: 1,
                    child: Text(
                      'State',
                      style: GoogleFonts.roboto(
                        fontSize: 10, // Reduced font size
                        fontWeight: FontWeight.w500,
                        color: Colors.purple[800],
                      ),
                      overflow: TextOverflow.ellipsis,
                    ),
                  ),
                  Transform.scale(
                    scale: 0.8, // Scale down the Switch
                    child: Switch(
                      value: relayStates[index],
                      onChanged: (value) => toggleRelay(index, value),
                      activeColor: Colors.purple[600],
                      inactiveThumbColor: Colors.grey[400],
                      inactiveTrackColor: Colors.grey[200],
                      materialTapTargetSize: MaterialTapTargetSize.shrinkWrap,
                    ),
                  ),
                ],
              ),
            ],
          ),
        ),
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      body: SafeArea(
        child: SingleChildScrollView(
          padding: const EdgeInsets.fromLTRB(15, 30, 15, 20),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.center,
            children: [
              Text(
                'Set Relay Times',
                style: GoogleFonts.roboto(
                  fontSize: 26,
                  fontWeight: FontWeight.bold,
                  color: Colors.purple[900],
                ),
              ),
              const SizedBox(height: 15),
              GridView.count(
                crossAxisCount: 2, // Two columns for 2x2 layout
                shrinkWrap: true,
                physics: const NeverScrollableScrollPhysics(),
                mainAxisSpacing: 10,
                crossAxisSpacing: 10,
                childAspectRatio: 0.8, // Increased for more vertical space
                children: List.generate(4, (index) {
                  return _buildRelayControl(index, context);
                }),
              ),
              const SizedBox(height: 25),
              ElevatedButton(
                onPressed: sendSettingsToESP,
                style: ElevatedButton.styleFrom(
                  backgroundColor: Colors.purple[600],
                  foregroundColor: Colors.white,
                  shape: RoundedRectangleBorder(
                    borderRadius: BorderRadius.circular(15),
                  ),
                  padding:
                      const EdgeInsets.symmetric(horizontal: 20, vertical: 12),
                  elevation: 4,
                  minimumSize: const Size(150, 48),
                ),
                child: Row(
                  mainAxisSize: MainAxisSize.min,
                  children: [
                    const Icon(Icons.save, size: 20, color: Colors.white),
                    const SizedBox(width: 8),
                    Text(
                      'Set All Times',
                      style: GoogleFonts.roboto(
                        fontSize: 16,
                        fontWeight: FontWeight.w600,
                      ),
                    ),
                  ],
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }
}

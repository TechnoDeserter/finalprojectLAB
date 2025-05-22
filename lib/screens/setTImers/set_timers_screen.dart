import 'package:flutter/material.dart';
import 'package:google_fonts/google_fonts.dart';
import 'package:speech_to_text/speech_to_text.dart' as stt;
import 'package:permission_handler/permission_handler.dart';

class SetTimeRelays extends StatefulWidget {
  final List<TimeOfDay> onTimes;
  final List<TimeOfDay> offTimes;
  final List<bool> relayStates;
  final Function(BuildContext, int, bool, {TimeOfDay? voiceSelectedTime}) selectTime;
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

  @override
  _SetTimeRelaysState createState() => _SetTimeRelaysState();
}

class _SetTimeRelaysState extends State<SetTimeRelays> {
  late stt.SpeechToText _speech;
  bool _speechRecognitionAvailable = false;
  bool _isListening = false;
  String _transcription = '';
  String _statusMessage = 'Tap to speak';

  @override
  void initState() {
    super.initState();
    _speech = stt.SpeechToText();
    _initializeSpeech();
  }

  void _initializeSpeech() async {
    try {
      bool available = await _speech.initialize(
        onStatus: (status) {
          print('Speech status: $status');
          setState(() {
            _isListening = status == 'listening';
            _statusMessage = _isListening ? 'Listening...' : 'Tap to speak';
          });
        },
        onError: (error) {
          print('Speech error: $error');
          setState(() {
            _isListening = false;
            _statusMessage = 'Error: ${error.errorMsg}';
          });
          ScaffoldMessenger.of(context).showSnackBar(
            SnackBar(
                content: Text('Speech recognition error: ${error.errorMsg}')),
          );
        },
      );
      setState(() {
        _speechRecognitionAvailable = available;
      });
      print('Speech recognition available: $available');
    } catch (e) {
      print('Error initializing speech recognition: $e');
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('Failed to initialize speech recognition: $e')),
      );
    }
  }

  void _showVoiceModal(BuildContext context) {
    showDialog(
      context: context,
      builder: (BuildContext dialogContext) {
        return StatefulBuilder(
          builder: (BuildContext context, StateSetter setModalState) {
            return AlertDialog(
              shape: RoundedRectangleBorder(
                borderRadius: BorderRadius.circular(15),
              ),
              title: Text(
                'Voice Command',
                style: GoogleFonts.roboto(
                  fontSize: 20,
                  fontWeight: FontWeight.bold,
                  color: Colors.purple[900],
                ),
              ),
              content: Column(
                mainAxisSize: MainAxisSize.min,
                children: [
                  Icon(
                    _isListening ? Icons.mic : Icons.mic_none,
                    size: 48,
                    color: _isListening ? Colors.red[600] : Colors.purple[600],
                  ),
                  const SizedBox(height: 10),
                  Text(
                    _statusMessage,
                    style: GoogleFonts.roboto(
                      fontSize: 16,
                      color: Colors.purple[700],
                    ),
                  ),
                  const SizedBox(height: 10),
                  Text(
                    _transcription.isEmpty
                        ? 'Say something like "Set relay 1 on time to 08:30"'
                        : _transcription,
                    style: GoogleFonts.roboto(
                      fontSize: 14,
                      color: Colors.grey[800],
                    ),
                    textAlign: TextAlign.center,
                  ),
                ],
              ),
              actions: [
                TextButton(
                  onPressed: () {
                    if (_isListening) {
                      _stopListening();
                    }
                    Navigator.of(dialogContext).pop();
                  },
                  child: Text(
                    _isListening ? 'Stop' : 'Close',
                    style: GoogleFonts.roboto(
                      fontSize: 16,
                      color: Colors.purple[600],
                    ),
                  ),
                ),
                if (!_isListening)
                  TextButton(
                    onPressed: () {
                      setModalState(() {
                        _transcription = '';
                        _statusMessage = 'Tap to speak';
                      });
                      _startListening();
                    },
                    child: Text(
                      'Speak Again',
                      style: GoogleFonts.roboto(
                        fontSize: 16,
                        color: Colors.purple[600],
                      ),
                    ),
                  ),
              ],
            );
          },
        );
      },
    );
  }

  void _startListening() async {
    if (_speechRecognitionAvailable && !_isListening) {
      var status = await Permission.microphone.request();
      if (status.isGranted) {
        try {
          await _speech.listen(
            onResult: (result) {
              print('Recognition result: ${result.recognizedWords}');
              setState(() {
                _transcription = result.recognizedWords;
                if (result.finalResult) {
                  _isListening = false;
                  _statusMessage = 'Processing command...';
                  _processVoiceCommand(result.recognizedWords);
                }
              });
            },
            localeId: 'en_US',
          );
        } catch (e) {
          print('Error starting speech recognition: $e');
          setState(() {
            _statusMessage = 'Error: $e';
          });
          ScaffoldMessenger.of(context).showSnackBar(
            SnackBar(content: Text('Failed to start speech recognition: $e')),
          );
        }
      } else {
        setState(() {
          _statusMessage = 'Microphone permission denied';
        });
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(content: Text('Microphone permission denied')),
        );
      }
    } else {
      print('Speech recognition not available or already listening');
      setState(() {
        _statusMessage = 'Speech recognition not available';
      });
    }
  }

  void _stopListening() async {
    if (_isListening) {
      try {
        await _speech.stop();
      } catch (e) {
        print('Error stopping speech recognition: $e');
        setState(() {
          _statusMessage = 'Error stopping: $e';
        });
      }
    }
  }

  void _processVoiceCommand(String command) {
    print('Processing command: $command');
    final regex = RegExp(r'set relay (\d+) (on|off) time to (\d{2}):(\d{2})',
        caseSensitive: false);
    final match = regex.firstMatch(command);
    if (match != null) {
      final relayNumber = int.parse(match.group(1)!) - 1;
      final isOnTime = match.group(2)!.toLowerCase() == 'on';
      final hour = int.parse(match.group(3)!);
      final minute = int.parse(match.group(4)!);

      if (relayNumber >= 0 &&
          relayNumber < widget.onTimes.length &&
          hour >= 0 &&
          hour <= 23 &&
          minute >= 0 &&
          minute <= 59) {
        final time = TimeOfDay(hour: hour, minute: minute);
        widget.selectTime(context, relayNumber, isOnTime, voiceSelectedTime: time);
        setState(() {
          _statusMessage = 'Command processed successfully';
        });
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
              content: Text(
                  'Set Relay ${relayNumber + 1} ${isOnTime ? "ON" : "OFF"} time to ${_formatTime(time)}')),
        );
      } else {
        setState(() {
          _statusMessage = 'Invalid relay number or time';
        });
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(content: Text('Invalid relay number or time')),
        );
      }
    } else {
      setState(() {
        _statusMessage = 'Could not understand the command';
      });
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('Heard: $command')),
      );
    }
  }

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
          padding: const EdgeInsets.all(13.0),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            mainAxisSize: MainAxisSize.min,
            children: [
              Text(
                'Relay ${index + 1}',
                style: GoogleFonts.roboto(
                  fontSize: 13.5,
                  fontWeight: FontWeight.bold,
                  color: Colors.purple[900],
                ),
              ),
              const SizedBox(height: 3),
              Row(
                mainAxisAlignment: MainAxisAlignment.spaceBetween,
                children: [
                  Flexible(
                    flex: 1,
                    child: Text(
                      'ON',
                      style: GoogleFonts.roboto(
                        fontSize: 10,
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
                      onPressed: () => widget.selectTime(context, index, true),
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
                            _formatTime(widget.onTimes[index]),
                            style: GoogleFonts.roboto(
                              fontSize: 9.5,
                              color: Colors.purple[800],
                            ),
                          ),
                        ],
                      ),
                    ),
                  ),
                ],
              ),
              const SizedBox(height: 3),
              Row(
                mainAxisAlignment: MainAxisAlignment.spaceBetween,
                children: [
                  Flexible(
                    flex: 1,
                    child: Text(
                      'OFF',
                      style: GoogleFonts.roboto(
                        fontSize: 10,
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
                      onPressed: () => widget.selectTime(context, index, false),
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
                            _formatTime(widget.offTimes[index]),
                            style: GoogleFonts.roboto(
                              fontSize: 9.5,
                              color: Colors.purple[800],
                            ),
                          ),
                        ],
                      ),
                    ),
                  ),
                ],
              ),
              const SizedBox(height: 3),
              Row(
                mainAxisAlignment: MainAxisAlignment.spaceBetween,
                children: [
                  Flexible(
                    flex: 1,
                    child: Text(
                      'State',
                      style: GoogleFonts.roboto(
                        fontSize: 10,
                        fontWeight: FontWeight.w500,
                        color: Colors.purple[800],
                      ),
                      overflow: TextOverflow.ellipsis,
                    ),
                  ),
                  Transform.scale(
                    scale: 0.8,
                    child: Switch(
                      value: widget.relayStates[index],
                      onChanged: (value) => widget.toggleRelay(index, value),
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
          padding: const EdgeInsets.fromLTRB(15, 13, 15, 20),
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
                crossAxisCount: 2,
                shrinkWrap: true,
                physics: const NeverScrollableScrollPhysics(),
                mainAxisSpacing: 10,
                crossAxisSpacing: 10,
                childAspectRatio: 0.8,
                children: List.generate(4, (index) {
                  return _buildRelayControl(index, context);
                }),
              ),
              const SizedBox(height: 15),
              ElevatedButton(
                onPressed: () => _showVoiceModal(context),
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
                    const Icon(Icons.mic, size: 20, color: Colors.white),
                    const SizedBox(width: 8),
                    Text(
                      'Voice Command',
                      style: GoogleFonts.roboto(
                        fontSize: 16,
                        fontWeight: FontWeight.w600,
                      ),
                    ),
                  ],
                ),
              ),
              const SizedBox(height: 15),
              ElevatedButton(
                onPressed: widget.sendSettingsToESP,
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
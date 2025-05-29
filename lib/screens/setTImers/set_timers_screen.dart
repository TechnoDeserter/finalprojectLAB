import 'package:flutter/material.dart';
import 'package:google_fonts/google_fonts.dart';
import 'package:speech_to_text/speech_to_text.dart' as stt;
import 'package:permission_handler/permission_handler.dart';

class SetTimeRelays extends StatefulWidget {
  final List<TimeOfDay> onTimes;
  final List<TimeOfDay> offTimes;
  final List<bool> relayStates;
  final Function(BuildContext, int, bool, {TimeOfDay? voiceSelectedTime})
      selectTime;
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

class _SetTimeRelaysState extends State<SetTimeRelays>
    with SingleTickerProviderStateMixin {
  late stt.SpeechToText _speech;
  bool _speechRecognitionAvailable = false;
  bool _isListening = false;
  String _transcription = '';
  String _statusMessage = 'Initializing speech recognition...';
  late AnimationController _animationController;
  late Animation<double> _micAnimation;

  @override
  void initState() {
    super.initState();
    _speech = stt.SpeechToText();
    _initializeSpeech();
    _animationController = AnimationController(
      vsync: this,
      duration: const Duration(milliseconds: 1000),
    )..repeat(reverse: true);
    _micAnimation = Tween<double>(begin: 1.0, end: 1.3).animate(
      CurvedAnimation(parent: _animationController, curve: Curves.easeInOut),
    );
  }

  void _initializeSpeech() async {
    try {
      bool available = await _speech.initialize(
        onStatus: (status) {
          print('Speech status: $status');
          setState(() {
            _isListening = status == 'listening';
            _statusMessage = _isListening ? 'Listening...' : 'Tap to speak';
            if (_isListening) {
              _animationController.repeat(reverse: true);
            } else {
              _animationController.stop();
              _animationController.value = 1.0;
            }
          });
        },
        onError: (error) {
          print('Speech error: $error');
          setState(() {
            _isListening = false;
            _statusMessage = 'Speech error: ${error.errorMsg}';
            _animationController.stop();
            _animationController.value = 1.0;
          });
          ScaffoldMessenger.of(context).showSnackBar(
            SnackBar(
                content: Text('Speech recognition error: ${error.errorMsg}')),
          );
        },
        debugLogging: true,
      );

      setState(() {
        _speechRecognitionAvailable = available;
        _statusMessage =
            available ? 'Tap to speak' : 'Speech recognition unavailable';
      });

      if (!available) {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(
            content: Text(
                'Failed to initialize speech recognition. Please check device settings.'),
          ),
        );
      }

      print('Speech recognition available: $available');
    } catch (e) {
      print('Error initializing speech recognition: $e');
      setState(() {
        _speechRecognitionAvailable = false;
        _statusMessage = 'Failed to initialize speech recognition';
      });
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
                  AnimatedBuilder(
                    animation: _micAnimation,
                    builder: (context, child) {
                      return Transform.scale(
                        scale: _isListening ? _micAnimation.value : 1.0,
                        child: Icon(
                          _isListening ? Icons.mic : Icons.mic_none,
                          size: 48,
                          color: _isListening
                              ? Colors.red[600]
                              : Colors.purple[600],
                        ),
                      );
                    },
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
                        ? 'Say "Turn on Relay 1" or "Set Relay 1 off time to 08:30"'
                        : _transcription,
                    style: GoogleFonts.roboto(
                      fontSize: 14,
                      color: _transcription.isEmpty
                          ? Colors.grey[800]
                          : Colors.grey[600],
                      fontStyle: _transcription.isEmpty
                          ? FontStyle.italic
                          : FontStyle.normal,
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
                TextButton(
                  onPressed: _speechRecognitionAvailable
                      ? () {
                          setModalState(() {
                            _transcription = '';
                            _statusMessage = 'Tap to speak';
                          });
                          _startListening(setModalState);
                        }
                      : null,
                  child: Text(
                    _isListening ? 'Listening...' : 'Speak',
                    style: GoogleFonts.roboto(
                      fontSize: 16,
                      color: _speechRecognitionAvailable
                          ? Colors.purple[600]
                          : Colors.grey,
                    ),
                  ),
                ),
              ],
            );
          },
        );
      },
    ).then((_) {
      _stopListening();
      _animationController.stop();
      _animationController.value = 1.0;
      setState(() {
        _transcription = '';
        _statusMessage = _speechRecognitionAvailable
            ? 'Tap to speak'
            : 'Speech recognition unavailable';
      });
    });
  }

  void _startListening(StateSetter setModalState) async {
    if (!_speechRecognitionAvailable) {
      setState(() {
        _statusMessage = 'Speech recognition not available';
      });
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('Speech recognition not available')),
      );
      return;
    }

    if (_isListening) {
      print('Already listening');
      return;
    }

    var status = await Permission.microphone.request();
    if (status.isGranted) {
      try {
        setState(() {
          _isListening = true;
          _statusMessage = 'Listening...';
          _animationController.repeat(reverse: true);
        });
        await _speech.listen(
          onResult: (result) {
            print(
                'Recognition result: ${result.recognizedWords}, final: ${result.finalResult}');
            setModalState(() {
              _transcription = result.recognizedWords;
            });
            if (result.finalResult) {
              setState(() {
                _isListening = false;
                _statusMessage = 'Processing command...';
                _animationController.stop();
                _animationController.value = 1.0;
                _processVoiceCommand(result.recognizedWords);
              });
            }
          },
          localeId: 'en_US',
          cancelOnError: true,
          partialResults: true,
        );
      } catch (e) {
        print('Error starting speech recognition: $e');
        setState(() {
          _isListening = false;
          _statusMessage = 'Error: $e';
          _animationController.stop();
          _animationController.value = 1.0;
        });
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('Failed to start speech recognition: $e')),
        );
      }
    } else {
      setState(() {
        _isListening = false;
        _statusMessage = 'Microphone permission denied';
        _animationController.stop();
        _animationController.value = 1.0;
      });
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(
            content: Text(
                'Microphone permission denied. Please enable in settings.')),
      );
      await openAppSettings();
    }
  }

  void _stopListening() async {
    if (_isListening) {
      try {
        await _speech.stop();
        setState(() {
          _isListening = false;
          _statusMessage = _speechRecognitionAvailable
              ? 'Tap to speak'
              : 'Speech recognition unavailable';
          _animationController.stop();
          _animationController.value = 1.0;
        });
      } catch (e) {
        print('Error stopping speech recognition: $e');
        setState(() {
          _isListening = false;
          _statusMessage = 'Error stopping: $e';
          _animationController.stop();
          _animationController.value = 1.0;
        });
      }
    }
  }

  void _processVoiceCommand(String command) {
    print('Processing command: $command');
    // Normalize command: convert to lowercase, handle common mis-transcriptions
    command = command.toLowerCase().trim();
    command = command
        // Normalize single-digit numbers
        .replaceAll('zero', '0')
        .replaceAll('oh', '0') // Handle "oh" for zero
        .replaceAll('one', '1')
        .replaceAll('two', '2')
        .replaceAll('three', '3')
        .replaceAll('tree', '3') // Handle mis-transcription
        .replaceAll('four', '4')
        .replaceAll('for', '4') // Handle mis-transcription
        .replaceAll('five', '5')
        .replaceAll('six', '6')
        .replaceAll('seven', '7')
        .replaceAll('eight', '8')
        .replaceAll('nine', '9')
        // Normalize tens
        .replaceAll('ten', '10')
        .replaceAll('of', 'OFF')
        .replaceAll('eleven', '11')
        .replaceAll('twelve', '12')
        .replaceAll('thirteen', '13')
        .replaceAll('fourteen', '14')
        .replaceAll('fifteen', '15')
        .replaceAll('sixteen', '16')
        .replaceAll('seventeen', '17')
        .replaceAll('eighteen', '18')
        .replaceAll('nineteen', '19')
        .replaceAll('twenty', '20')
        .replaceAll('twenty one', '21')
        .replaceAll('twenty two', '22')
        .replaceAll('twenty three', '23')
        // Handle minutes
        .replaceAll('hundred', ':00') // e.g., "fourteen hundred" -> "14:00"
        .replaceAll('ten', ':10') // Replace after single-digit numbers
        .replaceAll('twenty', ':20')
        .replaceAll('thirty', ':30')
        .replaceAll('forty', ':40')
        .replaceAll('fifty', ':50')
        // Clean up spaces around colon
        .replaceAll(' : ', ':');

    // Regex for scheduling command with "and": "set relay X on/off time to HH and MM"
    final andScheduleRegex = RegExp(
      r'set relay (\d+) (on|off) time to (\d{1,2}) and (\d{1,2})',
      caseSensitive: false,
    );
    final andScheduleMatch = andScheduleRegex.firstMatch(command);

    // Regex for scheduling command: "set relay X on/off time to HH:MM"
    final scheduleRegex = RegExp(
      r'set relay (\d+) (on|off) time to (\d{1,2}):(\d{2})',
      caseSensitive: false,
    );
    final scheduleMatch = scheduleRegex.firstMatch(command);

    // Regex for toggle command: "turn on/off relay X"
    final toggleRegex = RegExp(
      r'turn (on|off) relay (\d+)',
      caseSensitive: false,
    );
    final toggleMatch = toggleRegex.firstMatch(command);

    if (toggleMatch != null) {
      final state = toggleMatch.group(1)! == 'on';
      final relayNumber = int.parse(toggleMatch.group(2)!) - 1;
      print('Toggle match: state=$state, relayNumber=$relayNumber');

      if (relayNumber >= 0 && relayNumber < widget.relayStates.length) {
        widget.toggleRelay(relayNumber, state);
        setState(() {
          _statusMessage = 'Command processed successfully';
        });
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
            content:
                Text('Turned Relay ${relayNumber + 1} ${state ? "ON" : "OFF"}'),
          ),
        );
      } else {
        setState(() {
          _statusMessage = 'Invalid relay number';
        });
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(content: Text('Invalid relay number')),
        );
      }
    } else if (andScheduleMatch != null) {
      final relayNumber = int.parse(andScheduleMatch.group(1)!) - 1;
      final isOnTime = andScheduleMatch.group(2)! == 'on';
      final hour = int.parse(andScheduleMatch.group(3)!);
      final minute = int.parse(andScheduleMatch.group(4)!);
      print(
          'And schedule match: relayNumber=$relayNumber, isOnTime=$isOnTime, time=$hour:$minute');

      if (relayNumber >= 0 &&
          relayNumber < widget.onTimes.length &&
          hour >= 0 &&
          hour <= 23 &&
          minute >= 0 &&
          minute <= 59) {
        final time = TimeOfDay(hour: hour, minute: minute);
        widget.selectTime(context, relayNumber, isOnTime,
            voiceSelectedTime: time);
        setState(() {
          _statusMessage = 'Command processed successfully';
        });
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
            content: Text(
              'Set Relay ${relayNumber + 1} ${isOnTime ? "ON" : "OFF"} time to ${_formatTime(time)}',
            ),
          ),
        );
      } else {
        setState(() {
          _statusMessage = 'Invalid relay number or time';
        });
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(content: Text('Invalid relay number or time')),
        );
      }
    } else if (scheduleMatch != null) {
      final relayNumber = int.parse(scheduleMatch.group(1)!) - 1;
      final isOnTime = scheduleMatch.group(2)! == 'on';
      final hour = int.parse(scheduleMatch.group(3)!);
      final minute = int.parse(scheduleMatch.group(4)!);
      print(
          'Schedule match: relayNumber=$relayNumber, isOnTime=$isOnTime, time=$hour:$minute');

      if (relayNumber >= 0 &&
          relayNumber < widget.onTimes.length &&
          hour >= 0 &&
          hour <= 23 &&
          minute >= 0 &&
          minute <= 59) {
        final time = TimeOfDay(hour: hour, minute: minute);
        widget.selectTime(context, relayNumber, isOnTime,
            voiceSelectedTime: time);
        setState(() {
          _statusMessage = 'Command processed successfully';
        });
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(
            content: Text(
              'Set Relay ${relayNumber + 1} ${isOnTime ? "ON" : "OFF"} time to ${_formatTime(time)}',
            ),
          ),
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
      print('No regex match for command: $command');
    }
  }

  String _formatTime(TimeOfDay time) {
    return '${time.hour.toString().padLeft(2, '0')}:${time.minute.toString().padLeft(2, '0')}';
  }

  @override
  void dispose() {
    _animationController.dispose();
    super.dispose();
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
                onPressed: _speechRecognitionAvailable
                    ? () {
                        _showVoiceModal(context);
                        Future.delayed(const Duration(milliseconds: 300), () {
                          _startListening((_) {});
                        });
                      }
                    : null,
                style: ElevatedButton.styleFrom(
                  backgroundColor: _speechRecognitionAvailable
                      ? Colors.purple[600]
                      : Colors.grey,
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
            ],
          ),
        ),
      ),
    );
  }
}

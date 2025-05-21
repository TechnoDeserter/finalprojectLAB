import 'dart:convert';
import 'package:flutter/material.dart';
import 'package:http/http.dart' as http;
import 'package:google_fonts/google_fonts.dart';
import 'package:provider/provider.dart';
import 'package:michaelesp32/screens/setTimers/set_timers_screen.dart';
import 'package:michaelesp32/screens/setup/setup_screen.dart';
import 'package:michaelesp32/common/widgets/navbar.dart';
import 'package:michaelesp32/services/esp32_service.dart';

void main() {
  runApp(
    MultiProvider(
      providers: [
        ChangeNotifierProvider(create: (_) => ESP32Service()),
      ],
      child: const RelayControlApp(),
    ),
  );
}

class RelayControlApp extends StatelessWidget {
  const RelayControlApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Relay Control',
      theme: ThemeData(
        primarySwatch: Colors.purple,
        useMaterial3: true,
        textTheme: GoogleFonts.robotoTextTheme(),
      ),
      debugShowCheckedModeBanner: false,
      home: const RelayControlPage(),
    );
  }
}

class RelayControlPage extends StatefulWidget {
  const RelayControlPage({super.key});

  @override
  State<RelayControlPage> createState() => _RelayControlPageState();
}

class _RelayControlPageState extends State<RelayControlPage> {
  int _selectedIndex = 0;

  List<bool> relayStates = [false, false, false, false];
  List<TimeOfDay> onTimes = [
    const TimeOfDay(hour: 8, minute: 0),
    const TimeOfDay(hour: 9, minute: 0),
    const TimeOfDay(hour: 10, minute: 0),
    const TimeOfDay(hour: 11, minute: 0),
  ];
  List<TimeOfDay> offTimes = [
    const TimeOfDay(hour: 18, minute: 0),
    const TimeOfDay(hour: 19, minute: 0),
    const TimeOfDay(hour: 20, minute: 0),
    const TimeOfDay(hour: 21, minute: 0),
  ];

  @override
  void initState() {
    super.initState();
  }

  void selectTime(BuildContext context, int relayIndex, bool isOnTime,
      {TimeOfDay? voiceSelectedTime}) {
    if (voiceSelectedTime != null) {
      setState(() {
        if (isOnTime) {
          onTimes[relayIndex] = voiceSelectedTime;
        } else {
          offTimes[relayIndex] = voiceSelectedTime;
        }
      });
      print('Updated state: onTimes=$onTimes, offTimes=$offTimes');
    } else {
      showTimePicker(
        context: context,
        initialTime: isOnTime ? onTimes[relayIndex] : offTimes[relayIndex],
      ).then((picked) {
        if (picked != null) {
          setState(() {
            if (isOnTime) {
              onTimes[relayIndex] = picked;
            } else {
              offTimes[relayIndex] = picked;
            }
          });
          print('Updated state: onTimes=$onTimes, offTimes=$offTimes');
        }
      });
    }
  }

  Future<void> _sendSettingsToESP(String ip) async {
    try {
      final Map<String, String> data = {
        'onTime1': _formatTime(onTimes[0]),
        'offTime1': _formatTime(offTimes[0]),
        'onTime2': _formatTime(onTimes[1]),
        'offTime2': _formatTime(offTimes[1]),
        'onTime3': _formatTime(onTimes[2]),
        'offTime3': _formatTime(offTimes[2]),
        'onTime4': _formatTime(onTimes[3]),
        'offTime4': _formatTime(offTimes[3]),
      };

      final response = await http
          .post(
            Uri.parse('http://$ip/set'),
            headers: {'Content-Type': 'application/json'},
            body: jsonEncode(data),
          )
          .timeout(
            const Duration(seconds: 5),
            onTimeout: () => http.Response('Timeout', 408),
          );

      if (response.statusCode == 200) {
        final responseData = jsonDecode(response.body);
        if (responseData['success'] == true) {
          ScaffoldMessenger.of(context).showSnackBar(
            const SnackBar(content: Text('Settings saved successfully!')),
          );
        } else {
          throw Exception('Failed to save settings: ${responseData['error']}');
        }
      } else {
        throw Exception('Failed to save settings: HTTP ${response.statusCode}');
      }
    } catch (e) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('Error saving settings: $e')),
      );
    }
  }

  Future<void> _toggleRelay(String ip, int index, bool value) async {
    try {
      final response = await http
          .post(
            Uri.parse('http://$ip/toggle'),
            headers: {'Content-Type': 'application/json'},
            body: jsonEncode({
              'relay': (index + 1).toString(),
              'state': value ? '1' : '0',
            }),
          )
          .timeout(
            const Duration(seconds: 5),
            onTimeout: () => http.Response('Timeout', 408),
          );

      if (response.statusCode == 200) {
        final responseData = jsonDecode(response.body);
        if (responseData['success'] == true) {
          setState(() {
            relayStates[index] = value;
          });
        } else {
          throw Exception('Failed to toggle relay: ${responseData['error']}');
        }
      } else {
        throw Exception('Failed to toggle relay: HTTP ${response.statusCode}');
      }
    } catch (e) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('Error toggling relay: $e')),
      );
    }
  }

  String _formatTime(TimeOfDay time) {
    return '${time.hour.toString().padLeft(2, '0')}:${time.minute.toString().padLeft(2, '0')}';
  }

  void _onNavBarTapped(int index) {
    setState(() {
      _selectedIndex = index;
    });
  }

  @override
  Widget build(BuildContext context) {
    return Consumer<ESP32Service>(
      builder: (context, esp32Service, child) {
        return Scaffold(
          appBar: AppBar(
            title: Text(
              'FINAL PROJECT',
              style: GoogleFonts.roboto(
                fontWeight: FontWeight.bold,
                color: Colors.white,
              ),
            ),
            centerTitle: true,
            backgroundColor: Colors.purple[600],
          ),
          body: _selectedIndex == 0
              ? SetTimeRelays(
                  onTimes: onTimes,
                  offTimes: offTimes,
                  relayStates: relayStates,
                  selectTime: selectTime,
                  toggleRelay: (index, value) => _toggleRelay(esp32Service.esp32IP ?? '192.168.4.1', index, value),
                  sendSettingsToESP: () => _sendSettingsToESP(esp32Service.esp32IP ?? '192.168.4.1'),
                )
              : const Setup(),
          bottomNavigationBar: NavBar(
            selectedIndex: _selectedIndex,
            onTap: _onNavBarTapped,
          ),
        );
      },
    );
  }
}
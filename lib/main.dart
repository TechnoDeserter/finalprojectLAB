import 'package:flutter/material.dart';
import 'package:http/http.dart' as http;
import 'package:google_fonts/google_fonts.dart';
import 'package:michaelesp32/screens/setTimers/set_timers_screen.dart';
import 'package:michaelesp32/screens/setup/setup_screen.dart';
import 'package:michaelesp32/common/widgets/navbar.dart';

void main() {
  runApp(const RelayControlApp());
}

class RelayControlApp extends StatelessWidget {
  const RelayControlApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Relay Control',
      theme: ThemeData(
        primarySwatch: Colors.purple, // Updated to match purple theme
        useMaterial3: true,
        textTheme: GoogleFonts.robotoTextTheme(), // Apply GoogleFonts globally
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
  String esp32Ip = "192.168.8.116";
  String ssid = "Loading...";
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
    _fetchInitialData();
  }

  Future<void> _fetchInitialData() async {
    try {
      final response = await http.get(Uri.parse('http://$esp32Ip/')).timeout(
            const Duration(seconds: 5),
            onTimeout: () => http.Response('Timeout', 408),
          );
      if (response.statusCode == 200) {
        setState(() {
          ssid = _extractSSID(response.body);
        });
      } else {
        setState(() {
          ssid = "Error: HTTP ${response.statusCode}";
        });
      }
    } catch (e) {
      setState(() {
        ssid = "Error connecting";
      });
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('Failed to fetch initial data: $e')),
      );
    }
  }

  String _extractSSID(String html) {
    try {
      final ssidStart = html.indexOf('SSID: ') + 6;
      final ssidEnd = html.indexOf('</p>', ssidStart);
      return html.substring(ssidStart, ssidEnd);
    } catch (e) {
      return "Unknown SSID";
    }
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

  Future<void> _sendSettingsToESP() async {
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
            Uri.parse('http://$esp32Ip/set'),
            body: data,
          )
          .timeout(
            const Duration(seconds: 5),
            onTimeout: () => http.Response('Timeout', 408),
          );

      if (response.statusCode == 200) {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(content: Text('Settings saved successfully!')),
        );
      } else {
        throw Exception('Failed to save settings: ${response.statusCode}');
      }
    } catch (e) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('Error: $e')),
      );
    }
  }

  Future<void> _toggleRelay(int index, bool value) async {
    try {
      final response = await http.post(
        Uri.parse('http://$esp32Ip/toggle'),
        body: {
          'relay': (index + 1).toString(),
          'state': value ? '1' : '0',
        },
      ).timeout(
        const Duration(seconds: 5),
        onTimeout: () => http.Response('Timeout', 408),
      );

      if (response.statusCode == 200) {
        setState(() {
          relayStates[index] = value;
        });
      } else {
        throw Exception('Failed to toggle relay: ${response.statusCode}');
      }
    } catch (e) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('Error toggling relay: $e')),
      );
    }
  }

  Future<void> _resetWiFi(String ssid, String password) async {
    try {
      final response = await http.post(
        Uri.parse('http://$esp32Ip/reset'),
        body: {
          'ssid': ssid,
          'password': password,
        },
      ).timeout(
        const Duration(seconds: 5),
        onTimeout: () => http.Response('Timeout', 408),
      );

      if (response.statusCode == 200) {
        setState(() {
          this.ssid = ssid;
        });
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('WiFi settings updated: SSID=$ssid')),
        );
      } else {
        throw Exception(
            'Failed to update WiFi settings: ${response.statusCode}');
      }
    } catch (e) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('Error updating WiFi: $e')),
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
              toggleRelay: _toggleRelay,
              sendSettingsToESP: _sendSettingsToESP,
            )
          : Setup(
              esp32Ip: esp32Ip,
              ssid: ssid,
              resetWiFi: _resetWiFi,
            ),
      bottomNavigationBar: NavBar(
        selectedIndex: _selectedIndex,
        onTap: _onNavBarTapped,
      ),
    );
  }
}

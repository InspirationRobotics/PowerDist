import serial
import serial.tools.list_ports

def establishPort(port=None):
  if port:
    pdb_port = port
  else:
    available_ports = list(serial.tools.list_ports.comports())
    for port in available_ports:
      if "ItsyBitsy M0 Express" in port.description:
        pdb_port = port.device
  
  return serial.Serial(port=pdb_port, baudrate=115200, timeout=1)


def getBattInfo(serial_device):

  if serial_device.in_waiting:
  
    output = serial_device.readline().decode('ascii').strip()
    output = output[0:len(output) - 1]
    incoming = output[output.rfind("["):].split(",")
    data = {"Timestamp" : incoming[0][1:], "Battery 1" : incoming[1], "Battery 2" : incoming[2]}
    return data
  else :
    return None


def main():

  serial_device = establishPort()

  while True: 
    getBattInfo(serial_device)


if __name__ == "__main__":
  main()
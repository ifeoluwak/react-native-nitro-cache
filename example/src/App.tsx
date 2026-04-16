import { useEffect, useState } from 'react';
import { Text, View, StyleSheet, Image } from 'react-native';
import { NitroCacheHybridObject } from 'react-native-nitro-cache';

// import { fetch } from 'react-native-nitro-fetch'

// const result = multiply(3, 7);

export default function App() {
  const [result, setResult] = useState(null);
  const get = async () => {
    const res = await NitroCacheHybridObject.getOrFetch(
      'https://join.ostrom.de/images/tariff-plang-header.back.png'
    );
    console.log('result from js ', res);
    if (res) {
      setResult(res.url);
    }
  };
  // const get = async () => {
  //   const result = await fetch('https://join.ostrom.de/images/tariff-plang-header.back.png');
  //   const json = await result.json();
  //   console.log("result from js 2 ", json);
  // };
  useEffect(() => {
    get();
  }, []);

  return (
    <View style={styles.container}>
      <Text>Result</Text>
      <Image
        // source={{ uri: 'https://join.ostrom.de/images/tariff-plang-header.back.png' }}
        source={{
          // uri: 'file:///Users/ifeoluwaking/Library/Developer/CoreSimulator/Devices/16AD3ADE-BDFC-4BBC-BE35-E07FBA45F087/data/Containers/Data/Application/CF0CB150-7F5E-45EA-8C90-DCF113C7D5D2/Library/Caches/nitro-cache/tariff-plang-header.back.png',
          // uri: 'file:///Users/ifeoluwaking/Library/Developer/CoreSimulator/Devices/16AD3ADE-BDFC-4BBC-BE35-E07FBA45F087/data/Containers/Data/Application/967B62B6-FC36-41DD-8224-12879899FEA8/Library/Caches/nitro-cache/tariff-plang-header.back.png',
          // uri: 'file:///Users/ifeoluwaking/Library/Developer/CoreSimulator/Devices/16AD3ADE-BDFC-4BBC-BE35-E07FBA45F087/data/Containers/Data/Application/967B62B6-FC36-41DD-8224-12879899FEA8/Library/Caches/nitro-cache/tariff-plang-header.back.png',
          // uri: 'file:///Users/ifeoluwaking/Library/Developer/CoreSimulator/Devices/16AD3ADE-BDFC-4BBC-BE35-E07FBA45F087/data/Containers/Data/Application/967B62B6-FC36-41DD-8224-12879899FEA8/Library/Caches/nitro-cache/tariff-plang-header.back.png'
          uri: `file://${result}`,
          // uri: `file://Users/ifeoluwaking/Library/Developer/CoreSimulator/Devices/16AD3ADE-BDFC-4BBC-BE35-E07FBA45F087/data/Containers/Data/Application/3652985E-8350-4393-BD13-3939CDF81969/Library/Caches/nitro-cache`,
          // uri: `file:///Users/ifeoluwaking/Library/Developer/CoreSimulator/Devices/16AD3ADE-BDFC-4BBC-BE35-E07FBA45F087/data/Containers/Data/Application/3652985E-8350-4393-BD13-3939CDF81969/Library/Caches/nitro-cache/tariff-plang-header.back.png`,
        }}
        onError={(e) => {
          console.log(e.nativeEvent.error);
        }}
        style={styles.image}
      />
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    alignItems: 'center',
    justifyContent: 'center',
  },
  image: {
    width: 100,
    height: 100,
  },
});

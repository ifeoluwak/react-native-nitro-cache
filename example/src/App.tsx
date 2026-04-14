import { useEffect } from 'react';
import { Text, View, StyleSheet, Image } from 'react-native';
import { NitroCacheHybridObject } from 'react-native-nitro-cache';

// import { fetch } from 'react-native-nitro-fetch'

// const result = multiply(3, 7);

export default function App() {
  const get = async () => {
    const result = await NitroCacheHybridObject.get(
      'https://join.ostrom.de/images/tariff-plang-header.back.png'
    );
    console.log('result from js ', result);
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
          uri: 'file:///Users/ifeoluwaking/Library/Developer/CoreSimulator/Devices/16AD3ADE-BDFC-4BBC-BE35-E07FBA45F087/data/Containers/Data/Application/4F090592-0FBA-4F22-8E9A-F5A5CDEB4BF5/Library/Caches/nitro-cache/tariff-plang-header.back.png',
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

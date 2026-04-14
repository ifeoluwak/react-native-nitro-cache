import { useEffect } from 'react';
import { Text, View, StyleSheet } from 'react-native';
import { NitroCacheHybridObject } from 'react-native-nitro-cache';

// const result = multiply(3, 7);

export default function App() {
  const get = async () => {
    const result = await NitroCacheHybridObject.get('https://www.google.com');
    console.log(result);
  };
  useEffect(() => {
    get();
  }, []);
  return (
    <View style={styles.container}>
      <Text>Result</Text>
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    alignItems: 'center',
    justifyContent: 'center',
  },
});
